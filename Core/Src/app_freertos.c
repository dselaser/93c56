/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "sk9822.h"
#include "voice_data.h"
#include "ee93c56.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern SPI_HandleTypeDef  hspi1;
extern I2S_HandleTypeDef  hi2s3;
extern UART_HandleTypeDef huart1;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* USER CODE BEGIN Definitions */
osThreadId_t mainAppTaskHandle;
const osThreadAttr_t mainAppTask_attributes = {
  .name = "mainAppTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 256 * 4
};

osThreadId_t detectTaskHandle;
const osThreadAttr_t detectTask_attributes = {
  .name = "detectTask",
  .priority = (osPriority_t) osPriorityBelowNormal,
  .stack_size = 256 * 4
};

/* detectTask → mainAppTask 공유 데이터 */
volatile bool     g_board_present;                /* 보드 감지 플래그 */
volatile uint8_t  g_detect_count;                 /* 감지된 칩 수 */
volatile bool     g_detected[EE_NUM_CHIPS];       /* 칩별 감지 상태 */
volatile bool     g_detect_enable;                /* 감지 태스크 활성화 */
volatile uint16_t g_skip_mask;                    /* 감지 스킵 마스크 (DIAG 후 설정) */

/* 고정값 쓰기 버퍼 (128 bytes, 93C46) */
static uint8_t  g_src_data[128];

/* ─── 보드 레이아웃 (4열×3행, U19-U30) ──────────────────────
 *   Row0: U19(CS0) U20(CS1) U21(CS2) U22(CS3)
 *   Row1: U23(CS4) U24(CS5) U25(CS6) U26(CS7)
 *   Row2: U27(CS8) U28(CS9) U29(CS10) U30(CS11)
 * ────────────────────────────────────────────────────────── */
#define PROG_SRC_CS   0u   /* U19: 템플릿 소스 슬롯 (최초 1회 읽기) */
/* USER CODE END Definitions */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartMainAppTask(void *argument);
void StartDetectTask(void *argument);
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* USER CODE END RTOS_QUEUES */

  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  mainAppTaskHandle  = osThreadNew(StartMainAppTask,  NULL, &mainAppTask_attributes);
  detectTaskHandle   = osThreadNew(StartDetectTask,   NULL, &detectTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* ── UART 로그 헬퍼 ──────────────────────────────────────────── */
static void uart_log(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0)
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
}

/* ── LED 유틸리티 ────────────────────────────────────────────── */

/* led_order[i]: 메모리 위치 i → SK9822 체인 인덱스
 * 메모리 보드 레이아웃 (4열 × 3행):
 *   Row0: U1  U2  U3  U4   → LED체인 0,1,2,3
 *   Row1: U5  U6  U7  U8   → LED체인 7,6,5,4 (역순)
 *   Row2: U9  U10 U11 U12  → LED체인 8,9,10,11
 */
static const uint8_t led_order[SK9822_NUM_LEDS] = {0,1,2,3,7,6,5,4,8,9,10,11};

static SK9822_Pixel g_leds[SK9822_NUM_LEDS];

static void led_all_off(void)
{
    memset(g_leds, 0, sizeof(g_leds));
    SK9822_Transmit(&hspi1, g_leds, SK9822_NUM_LEDS);
}

/* 메모리 위치 idx 의 LED를 r,g,b 색상으로 설정 (즉시 전송하지 않음) */
static void led_set(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t chain = led_order[idx];
    /* SK9822: brightness=0 → 완전 꺼짐. 색상에 따라 적절한 밝기 설정.
     * 청색(b채널)은 시감 보정으로 br=12, 소등=0, 나머지=4 */
    uint8_t br;
    if (r == 0 && g == 0 && b == 0) br = 0;   /* 소등 */
    else if (r == 0 && g == 0)      br = 12;   /* 청색 */
    else                             br = 4;    /* 적, 백, 기타 */
    g_leds[chain].brightness = br;
    g_leds[chain].r = r;
    g_leds[chain].g = g;
    g_leds[chain].b = b;
}

static void led_update(void)
{
    SK9822_Transmit(&hspi1, g_leds, SK9822_NUM_LEDS);
}

/* 모든 LED를 r,g,b 로 일괄 설정 + 전송 */
static void led_all_set(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < SK9822_NUM_LEDS; i++)
        led_set(i, r, g, b);
    led_update();
}

/* ── 음성 재생 (mainAppTask 에서 직접 I2S 출력) ─────────────── */

#define AUDIO_CHUNK_FRAMES  16
#define AUDIO_CHUNK_WORDS   (AUDIO_CHUNK_FRAMES * 2)  /* 스테레오 L+R */

static void voice_play(uint32_t clip_idx)
{
    if (clip_idx >= VOICE_NUM_CLIPS) return;

    const int16_t *src       = voice_clips[clip_idx].data;
    uint32_t       remaining = voice_clips[clip_idx].count;
    int16_t        buf[AUDIO_CHUNK_WORDS];

    while (remaining > 0)
    {
        uint32_t chunk = (remaining >= AUDIO_CHUNK_FRAMES)
                         ? AUDIO_CHUNK_FRAMES : remaining;

        for (uint32_t i = 0; i < chunk; i++) {
            buf[i * 2]     = src[i];   /* Left  */
            buf[i * 2 + 1] = src[i];   /* Right */
        }

        HAL_I2S_Transmit(&hi2s3, (uint16_t *)buf,
                         (uint16_t)(chunk * 2), HAL_MAX_DELAY);
        src       += chunk;
        remaining -= chunk;
    }
}

/* ── 메인 애플리케이션 상태 머신 ─────────────────────────────── */

/* ── 칩 존재 안전 확인 ────────────────────────────────────────
 * addr 0 에 0xA5 write+read 로 칩 응답 확인.
 * addr 0x7F(인식키)를 건드리지 않으므로 프로그래밍 후에도 안전. */
static bool ee_present_safe(uint8_t idx)
{
    EE_WriteEnable(idx);
    bool wr = EE_Write(idx, 0u, 0xA5u);
    EE_WriteDisable(idx);
    if (!wr) return false;
    return (EE_Read(idx, 0u) == 0xA5u);
}

/**
 * @brief 93C46 대량 프로그래밍 태스크
 *
 * 상태 흐름:
 *   INIT: detect task 초기화 대기 (g_num_addrs=128 확인, 최대 3초)
 *   A.   대기: LED 쇼, ee_present_safe(0)로 보드 삽입 감지
 *        템플릿 최초 1회 로드 (CS00 → g_src_data)
 *   D.   프로그래밍: CS00~CS11 전체 ERAL+Write+Verify
 *   E.   완료: "보드를 분리하세요" → ee_present_safe(0)로 제거 감지 → A로 복귀
 */
void StartMainAppTask(void *argument)
{
    bool prog_ok[EE_NUM_CHIPS];
    uint8_t pass_cnt, fail_cnt;
    static bool template_loaded = false;

    /* ── 전원 ON 인트로 ── */
    uart_log("\r\n=== 93C46 Mass Programmer (DSE INC) ===\r\n");
    uart_log("[INIT] Slots: %d, waiting for detect task...\r\n", EE_NUM_CHIPS);
    voice_play(VOICE_INTRO);
    osDelay(300);
    voice_play(VOICE_PLACE_BOARD);
    osDelay(300);

    /* detect task 초기화 완료 대기 (g_num_addrs=128, 최대 3초) */
    {
        uint32_t t0 = HAL_GetTick();
        while (g_num_addrs != 128u && (HAL_GetTick() - t0) < 3000u)
            osDelay(50u);
        if (g_num_addrs != 128u) {
            /* 보드 없이 시작 → 93C46 고정 설정 */
            g_addr_bits = 7u;
            g_num_addrs = 128u;
        }
    }
    g_detect_enable = false;   /* detect 주기 루프 비활성화 */
    uart_log("[INIT] addr_bits=%u, num_addrs=%u\r\n",
             (unsigned)g_addr_bits, (unsigned)g_num_addrs);

    for (;;)
    {
        /* ════════════════════════════════════════════════════════
         * 상태 A: LED 순차 점등 → LED 쇼 → 음성 반복
         * (보드 삽입 감지: ee_present_safe(CS00))
         * ════════════════════════════════════════════════════════ */

        /* ── 색상 팔레트 (등휘도 보정) ────────────────────── */
        static const struct { uint8_t r, g, b, br; } palette[] = {
            {255,   0,   0,  4},  /* 적 */
            {  0,   0, 255, 12},  /* 청 */
            {  0,  76,   0,  4},  /* 녹 */
            {200,  20,   0,  4},  /* 주황 */
            {  0,  50, 255,  4},  /* 청록 */
            {180,   0, 200,  4},  /* 자홍 */
            {255, 255,   0,  1},  /* 황 */
            {255, 255, 255,  2},  /* 백 */
        };
        #define PAL_N (sizeof(palette)/sizeof(palette[0]))

        /* ── 패턴 순서 테이블 ─────────────────────────── */
        /* 패턴 B: 세로 지그재그 */
        static const uint8_t patB[] = {0,7,8, 9,6,1, 2,5,10, 11,4,3};
        /* 패턴 D: 소용돌이 (외곽→중심) */
        static const uint8_t patD[] = {0,1,2,3, 4,11,10,9,8, 7, 6,5};
        /* 패턴 E: 확산 (중심→외곽) */
        static const uint8_t patE[] = {5,6, 1,7,9,2, 0,4,8,10,3,11};

        /* ── LED 쇼 헬퍼: 색상 1개 LED on-off ─────────── */
        #define SHOW_BLINK(pos, cr, cg, cb, cbr, on_ms, off_ms) do { \
            uint8_t _ci = led_order[pos]; \
            g_leds[_ci].brightness = cbr; \
            g_leds[_ci].r = cr; g_leds[_ci].g = cg; g_leds[_ci].b = cb; \
            led_update(); osDelay(on_ms); \
            led_set(pos, 0, 0, 0); \
            led_update(); osDelay(off_ms); \
        } while(0)

        /* (1) 순차 점등: U19→U30 백색 ON (200ms 간격) */
        uart_log("[WAIT] Waiting for board (ee_present_safe)...\r\n");
        led_all_off();
        for (uint8_t i = 0; i < SK9822_NUM_LEDS; i++) {
            led_set(i, 255, 255, 255);
            led_update();
            osDelay(200);
        }

        /* (2) 패턴 A~H 순환 쇼, 보드 감지 시 즉시 탈출 */
        bool board_found = ee_present_safe(0u);   /* 이미 꽂혀 있으면 즉시 탈출 */
        uint8_t pal_idx = 0;

        while (!board_found)
        {
/* ──────── 패턴 A: 번호순 왕복 (빠르게) ──────── */
#define PAT_COLOR palette[pal_idx % PAL_N]
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t i = 0; i < 12 && !board_found; i++) {
                    SHOW_BLINK(i, cr, cg, cb, cbr, 120, 30);
                    board_found = ee_present_safe(0u);
                }
                for (int8_t i = 11; i >= 0 && !board_found; i--) {
                    SHOW_BLINK((uint8_t)i, cr, cg, cb, cbr, 120, 30);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 B: 세로 지그재그 왕복 (보통) ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t s = 0; s < 12 && !board_found; s++) {
                    SHOW_BLINK(patB[s], cr, cg, cb, cbr, 130, 30);
                    board_found = ee_present_safe(0u);
                }
                for (int8_t s = 11; s >= 0 && !board_found; s--) {
                    SHOW_BLINK(patB[s], cr, cg, cb, cbr, 130, 30);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 C: 대각선 쓸기 ──────── */
            /* 대각선 그룹: {0},{1,7},{2,6,8},{3,5,9},{4,10},{11} */
            {
                static const uint8_t diag[][4] = {
                    {0,  255,255,255}, {1,7,  255,255}, {2,6,8, 255},
                    {3,5,9, 255},     {4,10, 255,255}, {11, 255,255,255}
                };
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                /* 순방향 */
                for (uint8_t d = 0; d < 6 && !board_found; d++) {
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++) {
                        uint8_t ci = led_order[diag[d][k]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(200);
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++)
                        led_set(diag[d][k], 0, 0, 0);
                    led_update(); osDelay(50);
                    board_found = ee_present_safe(0u);
                }
                /* 역방향 */
                for (int8_t d = 5; d >= 0 && !board_found; d--) {
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++) {
                        uint8_t ci = led_order[diag[d][k]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(200);
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++)
                        led_set(diag[d][k], 0, 0, 0);
                    led_update(); osDelay(50);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 D: 소용돌이 (외곽→중심) ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t s = 0; s < 12 && !board_found; s++) {
                    SHOW_BLINK(patD[s], cr, cg, cb, cbr, 150, 30);
                    board_found = ee_present_safe(0u);
                }
                for (int8_t s = 11; s >= 0 && !board_found; s--) {
                    SHOW_BLINK(patD[s], cr, cg, cb, cbr, 150, 30);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 E: 확산/수축 (중심→외곽→중심) ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t s = 0; s < 12 && !board_found; s++) {
                    SHOW_BLINK(patE[s], cr, cg, cb, cbr, 140, 40);
                    board_found = ee_present_safe(0u);
                }
                for (int8_t s = 11; s >= 0 && !board_found; s--) {
                    SHOW_BLINK(patE[s], cr, cg, cb, cbr, 140, 40);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 F: 랜덤 반짝 ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                uint32_t seed = HAL_GetTick();
                for (uint8_t n = 0; n < 24 && !board_found; n++) {
                    seed = seed * 1103515245U + 12345U;
                    uint8_t pos = (seed >> 16) % 12;
                    SHOW_BLINK(pos, cr, cg, cb, cbr, 60, 40);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 G: 행/열 스캔 ──────── */
            {
                /* 행 단위: row0={0,1,2,3}, row1={7,6,5,4}, row2={8,9,10,11} */
                static const uint8_t rows[3][4] = {
                    {0,1,2,3}, {7,6,5,4}, {8,9,10,11}
                };
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                /* 행 스캔 순방향 */
                for (uint8_t r = 0; r < 3 && !board_found; r++) {
                    for (uint8_t c = 0; c < 4; c++) {
                        uint8_t ci = led_order[rows[r][c]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(250);
                    for (uint8_t c = 0; c < 4; c++)
                        led_set(rows[r][c], 0, 0, 0);
                    led_update(); osDelay(80);
                    board_found = ee_present_safe(0u);
                }
                /* 열 스캔: col0={0,7,8}, col1={1,6,9}, col2={2,5,10}, col3={3,4,11} */
                static const uint8_t cols[4][3] = {
                    {0,7,8}, {1,6,9}, {2,5,10}, {3,4,11}
                };
                for (uint8_t c = 0; c < 4 && !board_found; c++) {
                    for (uint8_t r = 0; r < 3; r++) {
                        uint8_t ci = led_order[cols[c][r]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(250);
                    for (uint8_t r = 0; r < 3; r++)
                        led_set(cols[c][r], 0, 0, 0);
                    led_update(); osDelay(80);
                    board_found = ee_present_safe(0u);
                }
            }
            pal_idx++;
            if (board_found) break;

/* ──────── 패턴 H: 체커보드 교대 깜빡 ──────── */
            {
                /* 그룹1: 체스 흰칸 {0,2,5,7,9,11}, 그룹2: 흑칸 {1,3,4,6,8,10} */
                static const uint8_t grp1[] = {0,2,5,7,9,11};
                static const uint8_t grp2[] = {1,3,4,6,8,10};
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t n = 0; n < 8 && !board_found; n++) {
                    const uint8_t *on  = (n & 1) ? grp2 : grp1;
                    const uint8_t *off = (n & 1) ? grp1 : grp2;
                    for (uint8_t k = 0; k < 6; k++) {
                        uint8_t ci = led_order[on[k]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    for (uint8_t k = 0; k < 6; k++)
                        led_set(off[k], 0, 0, 0);
                    led_update();
                    osDelay(300);
                    board_found = ee_present_safe(0u);
                }
                led_all_off();
            }
            pal_idx++;
            if (board_found) break;

/* ──────── A~H 전체 완료 → 음성 출력 ──────── */
            voice_play(VOICE_PLACE_BOARD);

        } /* while (!board_found) */

        uart_log("[WAIT] Board detected! Starting...\r\n");
        led_all_off();
        osDelay(200);

        /* ── 템플릿 로드 (최초 1회) ────────────────────────── */
        if (!template_loaded) {
            uart_log("[INIT] Loading template from CS%02u...\r\n", PROG_SRC_CS);
            for (uint16_t a = 0u; a < 128u; a++)
                g_src_data[a] = EE_Read(PROG_SRC_CS, a);
            template_loaded = true;
            uart_log("[INIT] Template loaded. addr 0x7F=0x%02X\r\n",
                     g_src_data[0x7Fu]);
            if (g_src_data[0x7Fu] != 0x91u && g_src_data[0x7Fu] != 0x92u)
                uart_log("[INIT] WARNING: unexpected type byte (exp 0x91/0x92)\r\n");
            /* 덤프 */
            for (uint16_t a = 0u; a < 128u; a++) {
                if ((a & 0x0Fu) == 0u) uart_log("  %03X: ", (unsigned)a);
                uart_log("%02X ", g_src_data[a]);
                if ((a & 0x0Fu) == 0x0Fu) uart_log("\r\n");
            }
        }

        /* ════════════════════════════════════════════════════════
         * 상태 D: 12개 칩 전체 프로그래밍 (CS00~CS11)
         *   각 칩: EWEN → ERAL → Write 128B → EWDS → Verify
         * ════════════════════════════════════════════════════════ */
        uart_log("[PROG] Programming CS00~CS%02u (%u chips)...\r\n",
                 EE_NUM_CHIPS - 1u, EE_NUM_CHIPS);
        pass_cnt = 0u;
        fail_cnt = 0u;
        for (uint8_t i = 0u; i < EE_NUM_CHIPS; i++)
            prog_ok[i] = false;

        for (uint8_t i = 0u; i < EE_NUM_CHIPS; i++) {
            led_set(i, 0, 0, 255); led_update();   /* 청색: 진행 중 */
            uart_log("[PROG] CS%02u: ", i);

            /* ── ERAL ── */
            EE_WriteEnable(i);
            if (!EE_EraseAll(i)) {
                EE_WriteDisable(i);
                led_set(i, 255, 0, 0); led_update();
                uart_log("ERAL TOUT\r\n");
                fail_cnt++;
                continue;
            }

            /* ── Write 128 bytes ── */
            bool wr_ok = true;
            for (uint16_t a = 0u; a < 128u && wr_ok; a++) {
                if (!EE_Write(i, a, g_src_data[a])) {
                    uart_log("WR TOUT a=0x%02X\r\n", (unsigned)a);
                    wr_ok = false;
                }
            }
            EE_WriteDisable(i);

            if (!wr_ok) {
                led_set(i, 255, 0, 0); led_update();
                fail_cnt++;
                continue;
            }

            /* ── Verify ── */
            uint16_t err_cnt = 0u;
            for (uint16_t a = 0u; a < 128u; a++) {
                uint8_t rd = EE_Read(i, a);
                if (rd != g_src_data[a]) {
                    if (err_cnt < 4u)
                        uart_log("\r\n[PROG]   ERR a=0x%02X exp=0x%02X got=0x%02X",
                                 (unsigned)a, g_src_data[a], rd);
                    err_cnt++;
                }
            }

            prog_ok[i] = (err_cnt == 0u);
            if (prog_ok[i]) {
                led_set(i, 0, 255, 0); led_update();   /* 녹색: 성공 */
                pass_cnt++;
                uart_log("OK\r\n");
            } else {
                led_set(i, 255, 0, 0); led_update();   /* 적색: 실패 */
                fail_cnt++;
                uart_log("FAIL(err=%u)\r\n", (unsigned)err_cnt);
            }
        }

        /* 결과 요약 */
        uart_log("[DONE] PASS=%u  FAIL=%u\r\n", pass_cnt, fail_cnt);
        if (fail_cnt > 0u) {
            uart_log("[DONE] Failed chips: ");
            for (uint8_t i = 0u; i < EE_NUM_CHIPS; i++)
                if (!prog_ok[i]) uart_log("CS%02u ", i);
            uart_log("\r\n");
        } else {
            uart_log("[DONE] All chips OK.\r\n");
        }

        /* ════════════════════════════════════════════════════════
         * 상태 E: 완료 → 보드 제거 대기
         * ════════════════════════════════════════════════════════ */
        voice_play(VOICE_COMPLETE);
        osDelay(300);
        if (fail_cnt > 0u) voice_play(VOICE_DEFECTIVE);

        uart_log("[WAIT] Remove the board...\r\n");
        voice_play(VOICE_REMOVE_BOARD);

        /* ee_present_safe(CS00) 폴링으로 보드 제거 감지 */
        {
            uint8_t vc = 0u;
            while (ee_present_safe(0u)) {
                osDelay(500u);
                if (++vc >= 60u) {
                    voice_play(VOICE_REMOVE_BOARD);
                    vc = 0u;
                }
            }
        }

        /* 보드 제거 완료 */
        uart_log("[BOARD] Board removed. Ready for next cycle.\r\n\r\n");
        voice_play(VOICE_BOARD_REMOVED);
        osDelay(300);

        /* 전체 소등 후 LED 쇼(상태 A)로 복귀 */
        led_all_off();
        osDelay(500);

    } /* for(;;) 무한 루프 끝 → 상태 A로 복귀 */
}

/**
 * @brief 보드 감지 태스크 (백그라운드)
 *
 * g_detect_enable == true 일 때 500ms 주기로 12개 메모리 스캔.
 * 감지 결과를 g_detected[], g_detect_count, g_board_present 에 기록.
 * mainAppTask 가 g_board_present 플래그를 읽어 상태 전환.
 */
void StartDetectTask(void *argument)
{
    uint8_t prev_count = 0;
    uint8_t stable = 0;          /* 연속 동일 결과 횟수 */

    /* ── 시작 진단 (한 번만 실행) ────────────────────────────────
     * mainAppTask 가 UART 헤더를 출력하고 voice_play 에 진입한 후
     * 실행되도록 500ms 대기. BelowNormal 우선순위이므로 Normal 태스크가
     * 블락(I2S/osDelay) 중일 때만 CPU 를 얻어 진단 출력. */
    osDelay(500);
    uart_log("[DIAG] === Start ===\r\n");

    /* 2. 칩별 EWEN + Write(0xA5,addr255) + Read 단계별 진단
     *    WR=TOUT → ee_wait_ready() 타임아웃 (DO 가 H 로 안 올라옴)
     *    WR=OK, RB≠0xA5 → 쓰기는 됐지만 읽기 값 불일치
     *    WR=OK, RB=0xA5 → DETECTED 정상 */
    /* 진단/감지에서 스킵할 슬롯 비트마스크.
     * bit N = 1 이면 CS(N) 슬롯을 타임아웃 없이 건너뜀.
     * CS00(bit0) = 클론 소스: 원본 데이터 보호를 위해 쓰기 금지. */
#define EE_CHIP_SKIP_MASK  0x0001u   /* CS00 쓰기 완전 금지 */

    /* 진단 중 TOUT 칩도 감지 루프에서 스킵 (20ms 낭비 방지).
     * 진단 후 EE_CHIP_SKIP_MASK | tout_mask 를 사용. */
    uint16_t tout_mask = EE_CHIP_SKIP_MASK;

    uart_log("[DIAG] -- Write/Read per chip (skip=0x%04X) --\r\n",
             (unsigned)EE_CHIP_SKIP_MASK);
    uint8_t diag_first_ok = 0xFF;   /* 첫 번째 응답 칩 인덱스 */
    for (uint8_t di = 0; di < EE_NUM_CHIPS; di++) {
        if (EE_CHIP_SKIP_MASK & (1u << di)) {
            uart_log("[DIAG] CS%02u: SKIP\r\n", di);
            continue;
        }
        EE_WriteEnable(di);
        bool    wr_ok = EE_Write(di, 255, 0xA5);
        uint8_t rb    = 0xFF;
        if (wr_ok) {
            rb = EE_Read(di, 255);
            if (diag_first_ok == 0xFF)
                diag_first_ok = di;
        } else {
            tout_mask |= (1u << di);   /* TOUT → 감지 루프도 스킵 */
        }
        EE_WriteDisable(di);
        uart_log("[DIAG] CS%02u: WR=%-4s  RB=0x%02X  %s\r\n",
                 di,
                 wr_ok ? "OK" : "TOUT",
                 rb,
                 (wr_ok && rb == 0xA5) ? "DETECTED" : "---");
    }
    /* CS00(클론 소스): 쓰기 없이 읽기로만 확인 — 원본 데이터 무손상.
     * tout_mask bit0 는 유지 → 주기 루프 EE_Detect(0) 도 영구 차단.
     * g_detected[0] 는 아래 결과 블록에서 직접 true 로 설정. */
    {
        uint8_t rd_test = EE_Read(0u, 255u);
        uart_log("[DIAG] CS00(SRC): READ-ONLY addr255=0x%02X → PRESENT (write blocked)\r\n",
                 rd_test);
    }

    uart_log("[DIAG] === Diagnostic End (detect_skip=0x%04X) ===\r\n",
             (unsigned)tout_mask);
    g_skip_mask = tout_mask;   /* mainAppTask 에서 사용 */

    /* 진단 결과를 즉시 g_detected[] 에 반영 (EE_Detect 안정성 루프 불필요) */
    {
        uint8_t chip_cnt = 0;
        for (uint8_t di = 0; di < EE_NUM_CHIPS; di++) {
            /* CS00(SRC)는 쓰기 감지 없이 항상 존재로 처리 */
            bool ok = (di == 0u) ? true : !(tout_mask & (1u << di));
            g_detected[di] = ok;
            if (ok) chip_cnt++;
        }
        g_detect_count = chip_cnt;
        if (chip_cnt > 0)
            g_board_present = true;
        uart_log("[DIAG] Chips found: %u / %u\r\n", chip_cnt, EE_NUM_CHIPS);
    }

    if (diag_first_ok == 0xFF) {
        uart_log("[DIAG] No chips detected — skipping verify/ID\r\n");
    } else {
        uint8_t fc = diag_first_ok;

        /* 클론 소스(CS00=0)를 보호: 파괴적 쓰기는 CS00이 아닌 다른 칩 우선 사용.
         * CS01 이상 중 첫 번째로 응답한 칩을 id_chip으로 선택.
         * 다른 칩이 없으면 부득이 fc(CS00) 사용. */
        uint8_t id_chip = fc;  /* 기본값: 없으면 폴백 */
        for (uint8_t _di = 1u; _di < EE_NUM_CHIPS; _di++) {
            if (!(tout_mask & (1u << _di))) {
                id_chip = _di;
                break;
            }
        }
        uart_log("[DIAG] ID writes → CS%u%s\r\n", id_chip,
                 (id_chip != 0u) ? " (SRC 보호)" : " (폴백: CS00)");

        /* 쓰기 실제 동작 확인: id_chip 에 0x5A 쓰기 후 읽기
         * - RB=0x5A → 쓰기 정상
         * - RB=0x48 → 쓰기 미동작 (EWEN 불량 또는 칩 WP 활성)
         * - RB=기타  → 읽기 타이밍 문제 */
        uart_log("[DIAG] -- Write verify (CS%u, 0x5A) --\r\n", id_chip);
        {
            EE_WriteEnable(id_chip);
            bool w2 = EE_Write(id_chip, 255, 0x5A);
            uint8_t rb2 = w2 ? EE_Read(id_chip, 255) : 0xFF;
            EE_WriteDisable(id_chip);
            uart_log("[DIAG] WR=%-4s  RB=0x%02X  %s\r\n",
                     w2 ? "OK" : "TOUT", rb2,
                     (w2 && rb2 == 0x5A) ? "WR OK" : "MISMATCH");
        }

        /* 3. 칩 종류: 93C46 고정 (7-bit addr, 128 bytes) */
        g_chip_type = EE_CHIP_93C46; g_addr_bits = 7; g_num_addrs = 128;
        uart_log("[DIAG] -- Chip type: 93C46 (fixed, 7-bit addr, 128 bytes) --\r\n");
    }

    for (;;)
    {
        if (!g_detect_enable) {
            prev_count = 0;
            stable = 0;
            osDelay(100);
            continue;
        }

        uint8_t count = 0;
        bool det[EE_NUM_CHIPS];

        /* ── 쓰기+읽기 감지 (EE_Detect: 0x00 프로그래밍 칩도 확실히 감지) ── */
        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
            if (tout_mask & (1u << i)) {   /* 정적 스킵 + TOUT 스킵 (CS00 포함) */
                det[i] = false;
                continue;
            }
            det[i] = EE_Detect(i);
            if (det[i]) count++;
        }
        /* CS00(SRC): 쓰기 금지 — EE_Detect 없이 항상 존재로 처리 */
        if (!det[0]) { det[0] = true; count++; }

        /* 안정 확인: 3회 연속 동일 결과여야 반영 */
        if (count == prev_count) {
            if (stable < 255) stable++;
        } else {
            stable = 0;
        }
        prev_count = count;

        if (stable >= 3) {
            g_detect_count = count;
            for (uint8_t i = 0; i < EE_NUM_CHIPS; i++)
                g_detected[i] = det[i];

            if (count > 0)
                g_board_present = true;
            else
                g_board_present = false;
        }

        osDelay(200);
    }
}

/* USER CODE END Application */
