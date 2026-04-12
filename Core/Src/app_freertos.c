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
extern SPI_HandleTypeDef hspi1;
extern I2S_HandleTypeDef hi2s3;
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
    g_leds[chain].brightness = 0;   /* 백색 밝기 최저 */
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

/**
 * @brief 93C56 메모리 보드 검사/프로그래밍 태스크
 *
 * 상태 흐름:
 *   A. 대기: "팁 보드를 올려주세요" 음성 출력, 보드 장착 감지
 *   B. 감지: 10ms 주기로 12개 메모리 인식 검사, LED 표시
 *   C. 검사: 0xAAAA/0x5555 write/read 테스트
 *   D. 프로그래밍: 0x0000 write + verify
 *   E. 완료: "보드를 분리하세요" → 보드 제거 감지 → A로 복귀
 */
void StartMainAppTask(void *argument)
{
    bool detected[EE_NUM_CHIPS];    /* 메모리 인식 상태 */
    bool test_ok[EE_NUM_CHIPS];     /* 테스트 결과 */
    bool prog_ok[EE_NUM_CHIPS];     /* 프로그래밍 결과 */
    uint32_t stable_count;          /* 연속 안정 감지 횟수 */
    uint32_t last_voice_tick;       /* 음성 반복 타이머 */
    bool any_defective;

    /* ── 전원 ON 인트로 (최초 1회) ──────────────── */
    voice_play(VOICE_INTRO);
    osDelay(300);
    voice_play(VOICE_PLACE_BOARD);
    osDelay(300);

    for (;;)
    {
        /* ════════════════════════════════════════════════════════
         * 상태 A: LED 순차 점등 → LED 쇼 → 음성 반복
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
        led_all_off();
        for (uint8_t i = 0; i < SK9822_NUM_LEDS; i++) {
            led_set(i, 255, 255, 255);
            led_update();
            osDelay(200);
        }

        /* (2) 패턴 A~H 순환 쇼, H 끝나면 음성 출력 */
        g_board_present = false;
        g_detect_enable = true;
        {
            uint8_t pal_idx = 0;

            while (!g_board_present)
            {
/* ──────── 패턴 A: 번호순 왕복 (빠르게) ──────── */
#define PAT_COLOR palette[pal_idx % PAL_N]
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t i = 0; i < 12 && !g_board_present; i++)
                    SHOW_BLINK(i, cr, cg, cb, cbr, 120, 30);
                for (int8_t i = 11; i >= 0 && !g_board_present; i--)
                    SHOW_BLINK((uint8_t)i, cr, cg, cb, cbr, 120, 30);
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 B: 세로 지그재그 왕복 (보통) ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t s = 0; s < 12 && !g_board_present; s++)
                    SHOW_BLINK(patB[s], cr, cg, cb, cbr, 130, 30);
                for (int8_t s = 11; s >= 0 && !g_board_present; s--)
                    SHOW_BLINK(patB[s], cr, cg, cb, cbr, 130, 30);
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 C: 대각선 쓸기 ──────── */
            /* 대각선 그룹: {0},{1,7},{2,6,8},{3,5,9},{4,10},{11} */
            {
                static const uint8_t diag[][4] = {
                    {0,  255,255,255}, {1,7,  255,255}, {2,6,8, 255},
                    {3,5,9, 255},     {4,10, 255,255}, {11, 255,255,255}
                };
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                /* 순방향 */
                for (uint8_t d = 0; d < 6 && !g_board_present; d++) {
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++) {
                        uint8_t ci = led_order[diag[d][k]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(200);
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++)
                        led_set(diag[d][k], 0, 0, 0);
                    led_update(); osDelay(50);
                }
                /* 역방향 */
                for (int8_t d = 5; d >= 0 && !g_board_present; d--) {
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++) {
                        uint8_t ci = led_order[diag[d][k]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(200);
                    for (uint8_t k = 0; k < 4 && diag[d][k] != 255; k++)
                        led_set(diag[d][k], 0, 0, 0);
                    led_update(); osDelay(50);
                }
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 D: 소용돌이 (외곽→중심) ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t s = 0; s < 12 && !g_board_present; s++)
                    SHOW_BLINK(patD[s], cr, cg, cb, cbr, 150, 30);
                for (int8_t s = 11; s >= 0 && !g_board_present; s--)
                    SHOW_BLINK(patD[s], cr, cg, cb, cbr, 150, 30);
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 E: 확산/수축 (중심→외곽→중심) ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t s = 0; s < 12 && !g_board_present; s++)
                    SHOW_BLINK(patE[s], cr, cg, cb, cbr, 140, 40);
                for (int8_t s = 11; s >= 0 && !g_board_present; s--)
                    SHOW_BLINK(patE[s], cr, cg, cb, cbr, 140, 40);
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 F: 랜덤 반짝 ──────── */
            {
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                uint32_t seed = HAL_GetTick();
                for (uint8_t n = 0; n < 24 && !g_board_present; n++) {
                    seed = seed * 1103515245U + 12345U;
                    uint8_t pos = (seed >> 16) % 12;
                    SHOW_BLINK(pos, cr, cg, cb, cbr, 60, 40);
                }
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 G: 행/열 스캔 ──────── */
            {
                /* 행 단위: row0={0,1,2,3}, row1={7,6,5,4}, row2={8,9,10,11} */
                static const uint8_t rows[3][4] = {
                    {0,1,2,3}, {7,6,5,4}, {8,9,10,11}
                };
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                /* 행 스캔 순방향 */
                for (uint8_t r = 0; r < 3 && !g_board_present; r++) {
                    for (uint8_t c = 0; c < 4; c++) {
                        uint8_t ci = led_order[rows[r][c]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(250);
                    for (uint8_t c = 0; c < 4; c++)
                        led_set(rows[r][c], 0, 0, 0);
                    led_update(); osDelay(80);
                }
                /* 열 스캔: col0={0,7,8}, col1={1,6,9}, col2={2,5,10}, col3={3,4,11} */
                static const uint8_t cols[4][3] = {
                    {0,7,8}, {1,6,9}, {2,5,10}, {3,4,11}
                };
                for (uint8_t c = 0; c < 4 && !g_board_present; c++) {
                    for (uint8_t r = 0; r < 3; r++) {
                        uint8_t ci = led_order[cols[c][r]];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr; g_leds[ci].g = cg; g_leds[ci].b = cb;
                    }
                    led_update(); osDelay(250);
                    for (uint8_t r = 0; r < 3; r++)
                        led_set(cols[c][r], 0, 0, 0);
                    led_update(); osDelay(80);
                }
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── 패턴 H: 체커보드 교대 깜빡 ──────── */
            {
                /* 그룹1: 체스 흰칸 {0,2,5,7,9,11}, 그룹2: 흑칸 {1,3,4,6,8,10} */
                static const uint8_t grp1[] = {0,2,5,7,9,11};
                static const uint8_t grp2[] = {1,3,4,6,8,10};
                uint8_t cr=PAT_COLOR.r, cg=PAT_COLOR.g, cb=PAT_COLOR.b, cbr=PAT_COLOR.br;
                for (uint8_t n = 0; n < 8 && !g_board_present; n++) {
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
                }
                led_all_off();
            }
            pal_idx++;
            if (g_board_present) break;

/* ──────── A~H 전체 완료 → 음성 출력 ──────── */
                voice_play(VOICE_PLACE_BOARD);

            } /* while (!g_board_present) */
        }
        g_detect_enable = false;  /* 감지 태스크 일시 중지 */

        /* 감지 결과를 로컬 배열로 복사 */
        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++)
            detected[i] = g_detected[i];

        /* ── 칩 종류 식별 (93C46 vs 93C56) ─────────── */
        {
            EE_ChipType chip = EE_IdentifyChip();
            if (chip == EE_CHIP_93C56)
                voice_play(VOICE_IS_93C56);
            else if (chip == EE_CHIP_93C46)
                voice_play(VOICE_IS_93C46);
            osDelay(500);
        }

        /* ════════════════════════════════════════════════════════
         * 상태 B: 보드 감지 확인 (최종 상태 표시)
         * ════════════════════════════════════════════════════════ */
        osDelay(200);
        /* 안정 확인을 위해 다시 한번 스캔 */
        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
            detected[i] = EE_Detect(i);
            if (detected[i])
                led_set(i, 255, 255, 255);
            else
                led_set(i, 255, 0, 0);
        }
        led_update();
        osDelay(500);

        /* ════════════════════════════════════════════════════════
         * 상태 C: 메모리 테스트 (0xAAAA/0x5555 write/read)
         * ════════════════════════════════════════════════════════ */
        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
            if (!detected[i]) {
                test_ok[i] = false;
                continue;
            }

            test_ok[i] = EE_TestMemory(i);

            if (test_ok[i])
                led_set(i, 255, 255, 255);  /* 백색: 이상 없음 */
            else
                led_set(i, 255, 0, 0);      /* 적색: 불량 */
            led_update();
        }
        osDelay(500);

        /* ════════════════════════════════════════════════════════
         * 상태 D: 0x0000 프로그래밍 + 검증
         * ════════════════════════════════════════════════════════ */
        any_defective = false;

        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
            if (!detected[i] || !test_ok[i]) {
                prog_ok[i] = false;
                led_set(i, 255, 0, 0);      /* 적색 유지 */
                any_defective = true;
                continue;
            }

            prog_ok[i] = EE_ProgramZero(i);

            if (prog_ok[i])
                led_set(i, 0, 0, 255);      /* 청색: 프로그래밍 완료 */
            else {
                led_set(i, 255, 0, 0);      /* 적색: 불량 */
                any_defective = true;
            }
            led_update();
        }
        led_update();

        /* ════════════════════════════════════════════════════════
         * 상태 E: 완료 → 결과 음성 + 보드 제거 대기
         * ════════════════════════════════════════════════════════ */

        /* 결과 음성 메시지 */
        voice_play(VOICE_COMPLETE);

        if (any_defective) {
            osDelay(300);
            voice_play(VOICE_DEFECTIVE);
        }

        /* "보드를 분리하여 주십시요" */
        osDelay(500);
        voice_play(VOICE_REMOVE_BOARD);

        /* 보드 제거 감지: detectTask 활용 */
        g_board_present = true;
        g_detect_enable = true;
        {
            uint8_t voice_count = 0;
            while (g_board_present) {
                osDelay(500);
                if (++voice_count >= 60) {   /* 30초마다 반복 */
                    voice_play(VOICE_REMOVE_BOARD);
                    voice_count = 0;
                }
            }
        }
        g_detect_enable = false;

        /* "보드가 분리되었습니다" */
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

        /* ── 읽기 전용 감지 (쓰기 없음 → 포그핀 불안정 시 안전) ── */
        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
            det[i] = EE_DetectReadOnly(i);
            if (det[i]) count++;
        }

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
