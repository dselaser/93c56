#ifndef EE93C56_H
#define EE93C56_H

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── 93C56 Configuration ─────────────────────────────────────── */
#define EE_NUM_CHIPS        12
#define EE_WORD_BITS        16      /* x16 organisation: 128 x 16-bit */
#define EE_ADDR_BITS        7       /* 7-bit address for x16 */
#define EE_NUM_ADDRS        128     /* 128 words per chip */

/* ── GPIO Pin Definitions ────────────────────────────────────── */

/* Microwire shared signals */
#define EE_CLK_PORT         GPIOA
#define EE_CLK_PIN          GPIO_PIN_8

#define EE_DIN_PORT         GPIOC       /* MCU output → 93C56 DI */
#define EE_DIN_PIN          GPIO_PIN_0

#define EE_DOUT_PORT        GPIOC       /* CD4067 output → MCU input */
#define EE_DOUT_PIN         GPIO_PIN_1

/* CD4067 Mux select lines */
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

/* ── Driver API ──────────────────────────────────────────────── */

/** Initialise all GPIO pins for 93C56 + mux (call once after HAL_Init) */
void EE_GPIO_Init(void);

/** Select mux channel (0-11) for reading DO from chip idx */
void EE_MuxSelect(uint8_t idx);

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

/** Read one 16-bit word from chip idx at given address */
uint16_t EE_Read(uint8_t idx, uint8_t addr);

/** Write one 16-bit word to chip idx at given address.
 *  Returns true if write completed successfully. */
bool EE_Write(uint8_t idx, uint8_t addr, uint16_t data);

/** Detect if chip idx is present (pogo pin connected).
 *  Tries a read and checks for valid response. */
bool EE_Detect(uint8_t idx);

/** Test all memory cells of chip idx by writing/reading 0xAAAA and 0x5555.
 *  Returns true if all cells pass. */
bool EE_TestMemory(uint8_t idx);

/** Program all addresses of chip idx to 0x0000.
 *  Returns true if verify passes. */
bool EE_ProgramZero(uint8_t idx);

#endif /* EE93C56_H */
