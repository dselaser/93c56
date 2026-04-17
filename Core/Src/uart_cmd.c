#include "uart_cmd.h"
#include "ee93c56.h"
#include "sk9822.h"
#include "voice_data.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── 외부 참조 ──────────────────────────────────────────────── */
extern SPI_HandleTypeDef  hspi1;
extern I2S_HandleTypeDef  hi2s3;
extern volatile bool      g_board_present;
extern volatile uint8_t   g_detect_count;
extern volatile bool      g_detected[EE_NUM_CHIPS];
extern volatile bool      g_detect_enable;

/* MCP 모드 플래그 */
volatile bool g_mcp_mode = false;

/* ── 내부 상수 ──────────────────────────────────────────────── */
#define CMD_BUF_SIZE  256
#define TX_BUF_SIZE   1024

/* ── LED (app_freertos.c 의 led_order 재선언) ────────────── */
static const uint8_t led_order[SK9822_NUM_LEDS] = {0,1,2,3,7,6,5,4,8,9,10,11};
static SK9822_Pixel cmd_leds[SK9822_NUM_LEDS];

/* ── 음성 재생 (G.711 μ-law LUT 디코드) ────────────────────── */
#define AUDIO_CHUNK_FRAMES  32
#define AUDIO_CHUNK_WORDS   (AUDIO_CHUNK_FRAMES * 2)

static void cmd_voice_play(uint32_t clip_idx)
{
    if (clip_idx >= VOICE_NUM_CLIPS) return;

    const VoiceClip *clip      = &voice_clips[clip_idx];
    const uint8_t   *src       = clip->ulaw;
    uint32_t         remaining = clip->sample_count;
    int16_t          buf[AUDIO_CHUNK_WORDS];

    while (remaining > 0u) {
        uint32_t chunk = (remaining >= AUDIO_CHUNK_FRAMES)
                         ? AUDIO_CHUNK_FRAMES : remaining;
        for (uint32_t i = 0u; i < chunk; i++) {
            int16_t sample = voice_ulaw_table[src[i]];
            buf[i * 2]     = sample;
            buf[i * 2 + 1] = sample;
        }
        HAL_I2S_Transmit(&hi2s3, (uint16_t *)buf,
                         (uint16_t)(chunk * 2), HAL_MAX_DELAY);
        src       += chunk;
        remaining -= chunk;
    }
}

/* ── UART 송수신 헬퍼 ──────────────────────────────────────── */

static char tx_buf[TX_BUF_SIZE];

static void uart_send(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

static void uart_send_line(const char *str)
{
    uart_send(str);
    uart_send("\n");
}

/* ── 16진수 파싱 헬퍼 ──────────────────────────────────────── */

static int hex_to_int(const char *s)
{
    return (int)strtol(s, NULL, 16);
}

/* ── 명령 핸들러 ──────────────────────────────────────────── */

static void cmd_status(void)
{
    const char *type_str = "UNKNOWN";
    if (g_chip_type == EE_CHIP_93C56)      type_str = "93C56";
    else if (g_chip_type == EE_CHIP_93C46) type_str = "93C46";

    snprintf(tx_buf, TX_BUF_SIZE, "OK BOARD=%d COUNT=%u TYPE=%s",
             g_board_present ? 1 : 0, g_detect_count, type_str);
    uart_send_line(tx_buf);
}

static void cmd_detect(void)
{
    bool det[EE_NUM_CHIPS];
    uint8_t count = 0;

    for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
        det[i] = EE_DetectReadOnly(i);
        if (det[i]) count++;
    }

    char *p = tx_buf;
    p += sprintf(p, "OK ");
    for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
        if (i > 0) *p++ = ',';
        *p++ = det[i] ? '1' : '0';
    }
    *p = '\0';
    uart_send_line(tx_buf);
}

static void cmd_identify(void)
{
    EE_ChipType chip = EE_IdentifyChip();
    if (chip == EE_CHIP_93C56)
        uart_send_line("OK 93C56");
    else if (chip == EE_CHIP_93C46)
        uart_send_line("OK 93C46");
    else
        uart_send_line("OK UNKNOWN");
}

static void cmd_ee_read(char *args)
{
    /* args: "<idx> <addr> [len]" */
    int idx = 0, addr = 0, len = 1;
    char *tok = strtok(args, " ");
    if (!tok) { uart_send_line("ERR MISSING_IDX"); return; }
    idx = atoi(tok);

    tok = strtok(NULL, " ");
    if (!tok) { uart_send_line("ERR MISSING_ADDR"); return; }
    addr = hex_to_int(tok);

    tok = strtok(NULL, " ");
    if (tok) len = atoi(tok);

    if (idx < 0 || idx >= EE_NUM_CHIPS) { uart_send_line("ERR INVALID_IDX"); return; }
    if (len < 1 || len > 256) { uart_send_line("ERR INVALID_LEN"); return; }

    char *p = tx_buf;
    p += sprintf(p, "OK");
    for (int i = 0; i < len; i++) {
        uint8_t val = EE_Read((uint8_t)idx, (uint8_t)(addr + i));
        p += sprintf(p, " %02X", val);
    }
    uart_send_line(tx_buf);
}

static void cmd_ee_write(char *args)
{
    /* args: "<idx> <addr> <hex1> [hex2] ..." */
    char *tok = strtok(args, " ");
    if (!tok) { uart_send_line("ERR MISSING_IDX"); return; }
    int idx = atoi(tok);

    tok = strtok(NULL, " ");
    if (!tok) { uart_send_line("ERR MISSING_ADDR"); return; }
    int addr = hex_to_int(tok);

    if (idx < 0 || idx >= EE_NUM_CHIPS) { uart_send_line("ERR INVALID_IDX"); return; }

    EE_WriteEnable((uint8_t)idx);

    bool ok = true;
    int offset = 0;
    while ((tok = strtok(NULL, " ")) != NULL) {
        uint8_t val = (uint8_t)hex_to_int(tok);
        if (!EE_Write((uint8_t)idx, (uint8_t)(addr + offset), val)) {
            ok = false;
            break;
        }
        offset++;
    }

    EE_WriteDisable((uint8_t)idx);

    if (offset == 0)
        uart_send_line("ERR MISSING_DATA");
    else if (ok)
        uart_send_line("OK");
    else
        uart_send_line("ERR WRITE_FAIL");
}

static void cmd_ee_dump(char *args)
{
    char *tok = strtok(args, " ");
    if (!tok) { uart_send_line("ERR MISSING_IDX"); return; }
    int idx = atoi(tok);
    if (idx < 0 || idx >= EE_NUM_CHIPS) { uart_send_line("ERR INVALID_IDX"); return; }

    /* 256바이트를 32바이트씩 나눠 전송 (TX 버퍼 제한) */
    uart_send("OK");
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        uint8_t val = EE_Read((uint8_t)idx, (uint8_t)a);
        char hex[4];
        sprintf(hex, " %02X", val);
        uart_send(hex);
    }
    uart_send("\n");
}

static void cmd_ee_test(char *args)
{
    char *tok = strtok(args, " ");
    if (!tok) { uart_send_line("ERR MISSING_IDX"); return; }
    int idx = atoi(tok);
    if (idx < 0 || idx >= EE_NUM_CHIPS) { uart_send_line("ERR INVALID_IDX"); return; }

    bool pass = EE_TestMemory((uint8_t)idx);
    uart_send_line(pass ? "OK PASS" : "OK FAIL");
}

static void cmd_ee_program(char *args)
{
    char *tok = strtok(args, " ");
    if (!tok) { uart_send_line("ERR MISSING_IDX"); return; }
    int idx = atoi(tok);
    if (idx < 0 || idx >= EE_NUM_CHIPS) { uart_send_line("ERR INVALID_IDX"); return; }

    bool pass = EE_ProgramZero((uint8_t)idx);
    uart_send_line(pass ? "OK" : "ERR PROGRAM_FAIL");
}

static void cmd_led_set(char *args)
{
    /* args: "<idx> <r> <g> <b> <brightness>" */
    int idx = 0, r = 0, g = 0, b = 0, bri = 0;
    if (sscanf(args, "%d %d %d %d %d", &idx, &r, &g, &b, &bri) < 5) {
        uart_send_line("ERR USAGE: LED_SET <idx> <r> <g> <b> <bri>");
        return;
    }
    if (idx < 0 || idx >= SK9822_NUM_LEDS) { uart_send_line("ERR INVALID_IDX"); return; }

    uint8_t chain = led_order[idx];
    cmd_leds[chain].r = (uint8_t)r;
    cmd_leds[chain].g = (uint8_t)g;
    cmd_leds[chain].b = (uint8_t)b;
    cmd_leds[chain].brightness = (uint8_t)(bri & 0x1F);
    SK9822_Transmit(&hspi1, cmd_leds, SK9822_NUM_LEDS);
    uart_send_line("OK");
}

static void cmd_led_all(char *args)
{
    int r = 0, g = 0, b = 0, bri = 0;
    if (sscanf(args, "%d %d %d %d", &r, &g, &b, &bri) < 4) {
        uart_send_line("ERR USAGE: LED_ALL <r> <g> <b> <bri>");
        return;
    }
    for (uint8_t i = 0; i < SK9822_NUM_LEDS; i++) {
        cmd_leds[i].r = (uint8_t)r;
        cmd_leds[i].g = (uint8_t)g;
        cmd_leds[i].b = (uint8_t)b;
        cmd_leds[i].brightness = (uint8_t)(bri & 0x1F);
    }
    SK9822_Transmit(&hspi1, cmd_leds, SK9822_NUM_LEDS);
    uart_send_line("OK");
}

static void cmd_led_clear(void)
{
    memset(cmd_leds, 0, sizeof(cmd_leds));
    SK9822_Transmit(&hspi1, cmd_leds, SK9822_NUM_LEDS);
    uart_send_line("OK");
}

static void cmd_voice(char *args)
{
    char *tok = strtok(args, " ");
    if (!tok) { uart_send_line("ERR MISSING_CLIP"); return; }
    int clip = atoi(tok);
    if (clip < 0 || clip >= VOICE_NUM_CLIPS) { uart_send_line("ERR INVALID_CLIP"); return; }

    cmd_voice_play((uint32_t)clip);
    uart_send_line("OK");
}

/* ── 명령 디스패치 ─────────────────────────────────────────── */

static void dispatch_command(char *line)
{
    /* 앞뒤 공백/CR/LF 제거 */
    while (*line == ' ' || *line == '\r' || *line == '\n') line++;
    size_t len = strlen(line);
    while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\r' || line[len-1] == '\n'))
        line[--len] = '\0';

    if (len == 0) return;

    /* 명령어와 인자 분리 */
    char *args = strchr(line, ' ');
    if (args) {
        *args = '\0';
        args++;
        while (*args == ' ') args++;
    }

    /* MCP 모드 자동 진입 (첫 명령 수신 시) */
    if (!g_mcp_mode) {
        g_mcp_mode = true;
        g_detect_enable = false;
    }

    if (strcmp(line, "STATUS") == 0)           cmd_status();
    else if (strcmp(line, "DETECT") == 0)       cmd_detect();
    else if (strcmp(line, "IDENTIFY") == 0)     cmd_identify();
    else if (strcmp(line, "EE_READ") == 0)      cmd_ee_read(args ? args : "");
    else if (strcmp(line, "EE_WRITE") == 0)     cmd_ee_write(args ? args : "");
    else if (strcmp(line, "EE_DUMP") == 0)      cmd_ee_dump(args ? args : "");
    else if (strcmp(line, "EE_TEST") == 0)      cmd_ee_test(args ? args : "");
    else if (strcmp(line, "EE_PROGRAM") == 0)   cmd_ee_program(args ? args : "");
    else if (strcmp(line, "LED_SET") == 0)      cmd_led_set(args ? args : "");
    else if (strcmp(line, "LED_ALL") == 0)      cmd_led_all(args ? args : "");
    else if (strcmp(line, "LED_CLEAR") == 0)    cmd_led_clear();
    else if (strcmp(line, "VOICE") == 0)        cmd_voice(args ? args : "");
    else {
        snprintf(tx_buf, TX_BUF_SIZE, "ERR UNKNOWN_CMD %s", line);
        uart_send_line(tx_buf);
    }
}

/* ── UART 명령 수신 루프 (FreeRTOS 태스크에서 호출) ─────── */

void UART_Cmd_Process(void)
{
    static char rx_buf[CMD_BUF_SIZE];
    uint16_t rx_pos = 0;
    uint8_t ch;

    for (;;) {
        /* 1바이트 수신 (블로킹, 100ms 타임아웃) */
        HAL_StatusTypeDef st = HAL_UART_Receive(&huart1, &ch, 1, 100);

        if (st == HAL_OK) {
            if (ch == '\n' || ch == '\r') {
                if (rx_pos > 0) {
                    rx_buf[rx_pos] = '\0';
                    dispatch_command(rx_buf);
                    rx_pos = 0;
                }
            } else {
                if (rx_pos < CMD_BUF_SIZE - 1)
                    rx_buf[rx_pos++] = (char)ch;
            }
        }
        /* 타임아웃 시 osDelay 대신 자연스럽게 다시 수신 대기 */
    }
}
