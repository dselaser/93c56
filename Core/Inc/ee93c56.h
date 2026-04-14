#ifndef EE93C56_H
#define EE93C56_H

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── 93C56 Configuration ─────────────────────────────────────── */
#define EE_NUM_CHIPS        12
#define EE_WORD_BITS        8       /* x8 organisation */

/* 93C46: 7-bit addr, 128 bytes  /  93C56: 8-bit addr, 256 bytes */
typedef enum {
    EE_CHIP_UNKNOWN = 0,
    EE_CHIP_93C46,      /* 1Kbit: 128 x 8 */
    EE_CHIP_93C56,      /* 2Kbit: 256 x 8 */
} EE_ChipType;

extern volatile EE_ChipType g_chip_type;
extern volatile uint8_t     g_addr_bits;   /* 7 or 8 */
extern volatile uint16_t    g_num_addrs;   /* 128 or 256 */

/* ── GPIO Pin Definitions ────────────────────────────────────── */

/* Microwire shared signals */
#define EE_CLK_PORT         GPIOA
#define EE_CLK_PIN          GPIO_PIN_8

#define EE_DIN_PORT         GPIOC       /* MCU output → 93C56 DI */
#define EE_DIN_PIN          GPIO_PIN_0

#define EE_DOUT_PORT        GPIOC       /* CD4067 output → MCU input */
#define EE_DOUT_PIN         GPIO_PIN_1

/* CD4067 Mux select lines
 * 주의: PCB 회로도에서 CD4067 Pin13(C)↔Pin14(D) 가 바뀌어 배선됨.
 *       소프트웨어에서 C/D GPIO 를 교차 정의하여 보상.
 *       MUX_C_PIN(PC9) → IC Pin14(실제 D), MUX_D_PIN(PC8) → IC Pin13(실제 C) */
#define MUX_A_PORT          GPIOC
#define MUX_A_PIN           GPIO_PIN_6
#define MUX_B_PORT          GPIOC
#define MUX_B_PIN           GPIO_PIN_7
#define MUX_C_PORT          GPIOC
#define MUX_C_PIN           GPIO_PIN_8
#define MUX_D_PORT          GPIOC
#define MUX_D_PIN           GPIO_PIN_9

#define MUX_INH_PORT        GPIOB       /* HIGH = mux output disabled */
#define MUX_INH_PIN         GPIO_PIN_12

/* Audio amp SD_MODE: R47 풀업으로 항상 ON → GPIO 제어 불필요 */

/* ── Chip Select Table ───────────────────────────────────────── */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} EE_CSPin;

extern const EE_CSPin ee_cs_table[EE_NUM_CHIPS];
extern const EE_CSPin ee_do_table[EE_NUM_CHIPS];  /* DO 직접 읽기 핀 */

/* ── Driver API ──────────────────────────────────────────────── */

/** Initialise all GPIO pins for 93C56 + mux (call once after HAL_Init) */
void EE_GPIO_Init(void);

/** Select mux channel for chip idx (PCB remap 적용) */
void EE_MuxSelect(uint8_t idx);

/** Select raw mux channel (0-15 직접 지정, 스캔/진단용) */
void EE_MuxSelectRaw(uint8_t ch);

/** Disable mux output (INHIBIT HIGH) */
void EE_MuxDisable(void);

/** Enable mux output (INHIBIT LOW) */
void EE_MuxEnable(void);

/** Assert/deassert chip-select for chip idx */
void EE_CS_High(uint8_t idx);
void EE_CS_Low(uint8_t idx);

/** Enable erase/write operations on chip idx */
void EE_WriteEnable(uint8_t idx);

/** Disable erase/write operations on chip idx */
void EE_WriteDisable(uint8_t idx);

/** Read one 8-bit byte from chip idx at given address */
uint8_t EE_Read(uint8_t idx, uint8_t addr);

/** Write one 8-bit byte to chip idx at given address.
 *  Returns true if write completed successfully. */
bool EE_Write(uint8_t idx, uint8_t addr, uint8_t data);

/** 전체 GPIO IDR 스캔: write 후 ready 상태에서 GPIOA~D IDR 읽기.
 *  out[0]=GPIOA, out[1]=GPIOB, out[2]=GPIOC, out[3]=GPIOD */
void EE_ScanAllGPIO(uint8_t idx, uint32_t out[4]);

/** Detect if chip idx is present (pogo pin connected).
 *  Tries a read and checks for valid response. */
bool EE_Detect(uint8_t idx);

/** DO 핀 직접 스캔: chip idx에 write 후 12개 DO 핀 전부 읽기.
 *  bit D = 1 → ee_do_table[D] 핀에서 HIGH 감지됨 */
uint16_t EE_ScanDOPins(uint8_t idx);

/** MUX 채널 매핑 스캔: chip idx에 write 후 ready 상태에서
 *  16채널 MUX 를 순회하여 DO=HIGH 가 읽히는 채널 비트마스크 반환.
 *  bit N = 1 → MUX channel N 에서 HIGH 감지됨 */
uint16_t EE_ScanMuxMapping(uint8_t idx);

/** 읽기 전용 감지 (쓰기 없음, 포그핀 불안정 시 안전). */
bool EE_DetectReadOnly(uint8_t idx);

/** 칩 종류 식별 (93C46 vs 93C56).
 *  보드 안정 감지 후 1회 호출. 결과를 g_chip_type 등에 설정.
 *  Returns: EE_CHIP_93C46 or EE_CHIP_93C56 or EE_CHIP_UNKNOWN */
EE_ChipType EE_IdentifyChip(void);

/** Test all memory cells of chip idx by writing/reading 0xAAAA and 0x5555.
 *  Returns true if all cells pass. */
bool EE_TestMemory(uint8_t idx);

/** Program all addresses of chip idx to 0x0000.
 *  Returns true if verify passes. */
bool EE_ProgramZero(uint8_t idx);

#endif /* EE93C56_H */
