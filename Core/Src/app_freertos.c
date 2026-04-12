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

    for (;;)
    {
        /* ════════════════════════════════════════════════════════
         * 상태 A: LED 순차 점등 → 외곽 회전 쇼 → 음성 반복
         * ════════════════════════════════════════════════════════ */

        /* 외곽 LED 시계방향 (위치 인덱스 = led_order 경유)
         *  U19=pos0, U20=pos1, U21=pos2, U22=pos3  (상단 L→R)
         *  U26=pos7                                 (우측)
         *  U30=pos11, U29=pos10, U28=pos9, U27=pos8 (하단 R→L)
         *  U23=pos4                                 (좌측)
         */
        static const uint8_t rim[] = {0,1,2,3, 4, 11,10,9,8, 7};

        /* (1) 순차 점등: 전체 LED 백색 ON (200ms 간격) */
        led_all_off();
        for (uint8_t i = 0; i < SK9822_NUM_LEDS; i++) {
            led_set(i, 255, 255, 255);
            led_update();
            osDelay(200);
        }

        /* (2) 외곽 회전 쇼 (detectTask가 보드 감지)
         *  5회전 순방향(색1) → 음성 → 5회전 역방향(색2) → 음성 → ... */

        /* 회전 색상 팔레트 (등휘도 보정)
         *
         * 인간 눈 감응도 (ITU-R BT.709):
         *   R × 0.2126 + G × 0.7152 + B × 0.0722 = 체감 밝기
         *   → 녹색이 가장 밝고, 청색이 가장 어둡게 느껴짐
         *
         * SK9822 brightness (0-31)로 보정:
         *   모든 색의 체감 밝기 ≈ 108 으로 통일
         */
        static const struct { uint8_t r, g, b, br; } show_colors[] = {
            {255,   0,   0,  4},  /* 적색   */
            {  0,   0, 255, 12},  /* 청색   */
            {  0,  76,   0,  4},  /* 녹색   */
            {200,  20,   0,  4},  /* 주황   */
            {  0,  50, 255,  4},  /* 청록   */
            {180,   0, 200,  4},  /* 자홍   */
        };
        #define NUM_SHOW_COLORS (sizeof(show_colors)/sizeof(show_colors[0]))

        g_board_present = false;
        g_detect_enable = true;
        {
            uint8_t color_idx = 0;
            bool forward = true;    /* true=순방향, false=역방향 */
            uint8_t show_count = 0;

            while (!g_board_present)
            {
                uint8_t cr = show_colors[color_idx].r;
                uint8_t cg = show_colors[color_idx].g;
                uint8_t cb = show_colors[color_idx].b;
                uint8_t cbr = show_colors[color_idx].br;

                /* ── 1회전 ──────────────────────────────── */
                for (uint8_t s = 0; s < 10; s++)
                {
                    uint8_t cur, prev;
                    if (forward) {
                        cur  = rim[s];
                        prev = rim[(s == 0) ? 9 : s - 1];
                    } else {
                        cur  = rim[9 - s];
                        prev = rim[(9 - s == 9) ? 0 : 10 - s];
                    }

                    led_set(prev, 255, 255, 255);  /* 이전 → 백색 (br=2) */
                    {   /* 현재 → 색상 (보정 brightness 적용) */
                        uint8_t ci = led_order[cur];
                        g_leds[ci].brightness = cbr;
                        g_leds[ci].r = cr;
                        g_leds[ci].g = cg;
                        g_leds[ci].b = cb;
                    }
                    led_update();
                    osDelay(200);

                    if (g_board_present) break;
                }
                /* 마지막 색상 복원 */
                if (forward)
                    led_set(rim[9], 255, 255, 255);
                else
                    led_set(rim[0], 255, 255, 255);
                led_update();

                /* ── 매 회전마다 색상 변경, 방향 교대 ── */
                forward = !forward;
                color_idx = (color_idx + 1) % NUM_SHOW_COLORS;

                /* ── 5회전마다 음성 출력 ─────────────── */
                if (!g_board_present && ++show_count >= 5) {
                    voice_play(VOICE_PLACE_BOARD);
                    show_count = 0;
                }
            }
        }
        g_detect_enable = false;  /* 감지 태스크 일시 중지 */

        /* 감지 결과를 로컬 배열로 복사 */
        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++)
            detected[i] = g_detected[i];

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

        osDelay(1000);
        voice_play(VOICE_REMOVE_BOARD);

        /* 보드 제거 감지: detectTask 활용 */
        g_board_present = true;
        g_detect_enable = true;
        {
            uint8_t voice_count = 0;
            while (g_board_present) {
                osDelay(500);
                /* 5회(2.5초)마다 음성 반복 체크 → 약 30초 간격 */
                if (++voice_count >= 60) {   /* 60 × 0.5s = 30s */
                    voice_play(VOICE_REMOVE_BOARD);
                    voice_count = 0;
                }
            }
        }
        g_detect_enable = false;

        /* 전체 소등 후 상태 A로 복귀 */
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
    for (;;)
    {
        if (!g_detect_enable) {
            osDelay(100);
            continue;
        }

        uint8_t count = 0;
        bool det[EE_NUM_CHIPS];

        for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
            det[i] = EE_Detect(i);
            if (det[i]) count++;
        }

        /* 결과 기록 (안정 확인: 2회 연속 동일 결과) */
        static uint8_t prev_count = 0;
        if (count == prev_count) {
            g_detect_count = count;
            for (uint8_t i = 0; i < EE_NUM_CHIPS; i++)
                g_detected[i] = det[i];

            if (count > 0)
                g_board_present = true;
            else
                g_board_present = false;
        }
        prev_count = count;

        osDelay(200);
    }
}

/* USER CODE END Application */
