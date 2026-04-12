/**
 * ee93c56.c — 93C56 Microwire EEPROM bit-bang driver
 *
 * 93C56 x16 mode: 128 words × 16-bit = 256 bytes per chip
 * 12 chips on memory board, accessed via CD4067 analog mux for DO lines.
 *
 * Microwire command format (x16):
 *   START_BIT(1) + OPCODE(2) + ADDRESS(7) [+ DATA(16)]
 *
 * Opcodes:
 *   READ  = 10   WRITE = 01   ERASE = 11
 *   EWEN  = 00 + 11xxxxx      EWDS  = 00 + 00xxxxx
 */

#include "ee93c56.h"

/* ── CS Pin Table ────────────────────────────────────────────── */
const EE_CSPin ee_cs_table[EE_NUM_CHIPS] = {
    { GPIOA, GPIO_PIN_9  },   /* CS0  - PA9  */
    { GPIOA, GPIO_PIN_10 },   /* CS1  - PA10 */
    { GPIOA, GPIO_PIN_11 },   /* CS2  - PA11 */
    { GPIOA, GPIO_PIN_12 },   /* CS3  - PA12 */
    { GPIOB, GPIO_PIN_4  },   /* CS4  - PB4  */
    { GPIOB, GPIO_PIN_5  },   /* CS5  - PB5  */
    { GPIOB, GPIO_PIN_6  },   /* CS6  - PB6  */
    { GPIOB, GPIO_PIN_7  },   /* CS7  - PB7  */
    { GPIOB, GPIO_PIN_9  },   /* CS8  - PB9  */
    { GPIOC, GPIO_PIN_11 },   /* CS9  - PC11 */
    { GPIOC, GPIO_PIN_12 },   /* CS10 - PC12 */
    { GPIOD, GPIO_PIN_2  },   /* CS11 - PD2  */
};

/* ── Timing Delay ────────────────────────────────────────────── */
/* SystemCoreClock = 32 MHz → ~31.25ns/cycle
 * 93C56 min clock period ≈ 1µs → 16 cycles per half-period */
static inline void ee_delay(void)
{
    /* ~0.5µs at 32 MHz (16 NOPs) */
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP();
}

/* ── Low-level GPIO helpers ──────────────────────────────────── */
static inline void clk_high(void)  { EE_CLK_PORT->BSRR = EE_CLK_PIN; }
static inline void clk_low(void)   { EE_CLK_PORT->BSRR = (uint32_t)EE_CLK_PIN << 16; }
static inline void din_high(void)  { EE_DIN_PORT->BSRR = EE_DIN_PIN; }
static inline void din_low(void)   { EE_DIN_PORT->BSRR = (uint32_t)EE_DIN_PIN << 16; }
static inline uint8_t dout_read(void) { return (EE_DOUT_PORT->IDR & EE_DOUT_PIN) ? 1 : 0; }

/* Send one bit on DI, clock it */
static void ee_send_bit(uint8_t bit)
{
    if (bit) din_high(); else din_low();
    ee_delay();
    clk_high();
    ee_delay();
    clk_low();
}

/* Read one bit from DO, clock it */
static uint8_t ee_recv_bit(void)
{
    clk_high();
    ee_delay();
    uint8_t bit = dout_read();
    clk_low();
    ee_delay();
    return bit;
}

/* Send start bit + opcode(2) + address(7) */
static void ee_send_command(uint8_t opcode, uint8_t addr)
{
    /* Start bit */
    ee_send_bit(1);
    /* Opcode: 2 bits MSB first */
    ee_send_bit((opcode >> 1) & 1);
    ee_send_bit(opcode & 1);
    /* Address: 7 bits MSB first */
    for (int i = EE_ADDR_BITS - 1; i >= 0; i--)
        ee_send_bit((addr >> i) & 1);
}

/* Wait for write/erase completion (DO goes HIGH when ready)
 * HAL_GetTick은 FreeRTOS에서 증가하지 않을 수 있으므로
 * 루프 카운터 기반 타임아웃 사용 (32MHz 기준) */
static bool ee_wait_ready(uint8_t idx, uint32_t timeout_ms)
{
    /* After write: CS LOW briefly, then CS HIGH to poll */
    EE_CS_Low(idx);
    ee_delay();
    EE_CS_High(idx);
    ee_delay();

    /* 32MHz 에서 루프 1회 ≈ 10 cycles → ~3,200 iterations/ms */
    volatile uint32_t loops = timeout_ms * 3200U;
    while (loops--) {
        if (dout_read())
            return true;   /* ready */
    }
    return false;          /* timeout */
}

/* ── GPIO Initialisation ─────────────────────────────────────── */
void EE_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Enable all port clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* ── Outputs: CLK, DIN ─────────────────────────────── */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = EE_CLK_PIN;
    HAL_GPIO_WritePin(EE_CLK_PORT, EE_CLK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(EE_CLK_PORT, &gpio);

    gpio.Pin = EE_DIN_PIN;
    HAL_GPIO_WritePin(EE_DIN_PORT, EE_DIN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(EE_DIN_PORT, &gpio);

    /* ── Input: DOUT (from CD4067 mux) ─────────────────── */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Pin  = EE_DOUT_PIN;
    HAL_GPIO_Init(EE_DOUT_PORT, &gpio);

    /* ── Outputs: Mux select A,B,C,D + INHIBIT ────────── */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = MUX_A_PIN;
    HAL_GPIO_WritePin(MUX_A_PORT, MUX_A_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(MUX_A_PORT, &gpio);

    gpio.Pin = MUX_B_PIN;
    HAL_GPIO_WritePin(MUX_B_PORT, MUX_B_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(MUX_B_PORT, &gpio);

    gpio.Pin = MUX_C_PIN;
    HAL_GPIO_WritePin(MUX_C_PORT, MUX_C_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(MUX_C_PORT, &gpio);

    gpio.Pin = MUX_D_PIN;
    HAL_GPIO_WritePin(MUX_D_PORT, MUX_D_PIN, GPIO_PIN_RESET);
    HAL_GPIO_Init(MUX_D_PORT, &gpio);

    /* INHIBIT = HIGH (mux disabled initially) */
    gpio.Pin = MUX_INH_PIN;
    HAL_GPIO_WritePin(MUX_INH_PORT, MUX_INH_PIN, GPIO_PIN_SET);
    HAL_GPIO_Init(MUX_INH_PORT, &gpio);

    /* ── Outputs: CS0-CS11 (all LOW initially) ─────────── */
    /* 주의: PB8 = SD_MODE (앰프), CS7은 PB7로 이동됨 */
    for (int i = 0; i < EE_NUM_CHIPS; i++) {
        gpio.Pin = ee_cs_table[i].pin;
        HAL_GPIO_WritePin(ee_cs_table[i].port, ee_cs_table[i].pin, GPIO_PIN_RESET);
        HAL_GPIO_Init(ee_cs_table[i].port, &gpio);
    }
}

/* ── Mux Control ─────────────────────────────────────────────── */
void EE_MuxSelect(uint8_t idx)
{
    HAL_GPIO_WritePin(MUX_A_PORT, MUX_A_PIN, (idx & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_B_PORT, MUX_B_PIN, (idx & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_C_PORT, MUX_C_PIN, (idx & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MUX_D_PORT, MUX_D_PIN, (idx & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    /* Allow mux settling time */
    ee_delay();
    ee_delay();
}

void EE_MuxDisable(void) { HAL_GPIO_WritePin(MUX_INH_PORT, MUX_INH_PIN, GPIO_PIN_SET); }
void EE_MuxEnable(void)  { HAL_GPIO_WritePin(MUX_INH_PORT, MUX_INH_PIN, GPIO_PIN_RESET); }

/* ── Chip Select ─────────────────────────────────────────────── */
void EE_CS_High(uint8_t idx)
{
    if (idx < EE_NUM_CHIPS)
        HAL_GPIO_WritePin(ee_cs_table[idx].port, ee_cs_table[idx].pin, GPIO_PIN_SET);
}

void EE_CS_Low(uint8_t idx)
{
    if (idx < EE_NUM_CHIPS)
        HAL_GPIO_WritePin(ee_cs_table[idx].port, ee_cs_table[idx].pin, GPIO_PIN_RESET);
}

/* ── Write Enable / Disable ──────────────────────────────────── */
void EE_WriteEnable(uint8_t idx)
{
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* EWEN: SB(1) + 00 + 11_00000 */
    ee_send_bit(1);     /* start */
    ee_send_bit(0);     /* opcode 00 */
    ee_send_bit(0);
    ee_send_bit(1);     /* 11xxxxx */
    ee_send_bit(1);
    for (int i = 0; i < 5; i++)
        ee_send_bit(0);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();
}

void EE_WriteDisable(uint8_t idx)
{
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* EWDS: SB(1) + 00 + 00_00000 */
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(0);
    for (int i = 0; i < 7; i++)
        ee_send_bit(0);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();
}

/* ── Read ────────────────────────────────────────────────────── */
uint16_t EE_Read(uint8_t idx, uint8_t addr)
{
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* READ: opcode = 10 */
    ee_send_command(0x02, addr & 0x7F);

    /* Dummy bit (leading zero before data) */
    ee_recv_bit();

    /* Read 16-bit data MSB first */
    uint16_t data = 0;
    for (int i = 15; i >= 0; i--)
        data |= ((uint16_t)ee_recv_bit() << i);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();

    return data;
}

/* ── Write ───────────────────────────────────────────────────── */
bool EE_Write(uint8_t idx, uint8_t addr, uint16_t data)
{
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* WRITE: opcode = 01 */
    ee_send_command(0x01, addr & 0x7F);

    /* Send 16-bit data MSB first */
    for (int i = 15; i >= 0; i--)
        ee_send_bit((data >> i) & 1);

    /* Wait for write completion */
    bool ok = ee_wait_ready(idx, 20);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();

    return ok;
}

/* ── Detect ──────────────────────────────────────────────────── */
bool EE_Detect(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return false;

    /* Try to read address 0.
     * If no chip present, DOUT is pulled low by R45/R46 → all zeros.
     * A valid chip would respond with the start bit protocol.
     *
     * We also try a second read from a different address.
     * If both reads return the same value AND the dummy bit was 0
     * (proper protocol), the chip is present. */

    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* Send READ command for addr 0 */
    ee_send_command(0x02, 0);

    /* Check dummy bit: should be 0 if chip is responding */
    uint8_t dummy = ee_recv_bit();

    /* Read 16 bits */
    uint16_t val = 0;
    for (int i = 15; i >= 0; i--)
        val |= ((uint16_t)ee_recv_bit() << i);

    EE_CS_Low(idx);
    ee_delay();

    /* If no chip: CLK pulses on floating line → dummy might be random.
     * With pull-down on DOUT, no-chip gives dummy=0, val=0x0000.
     * A real chip also gives dummy=0 but could have val=0x0000.
     *
     * Better detection: write a test pattern, read back.
     * But we want non-destructive detection first. */

    /* Heuristic: try writing EWEN then reading.
     * If chip is present, EWEN succeeds silently.
     * Then write 0x1234 to addr 127 (unlikely data), read back. */

    /* Write enable */
    EE_CS_High(idx);
    ee_delay();
    ee_send_bit(1); ee_send_bit(0); ee_send_bit(0);  /* SB + 00 */
    ee_send_bit(1); ee_send_bit(1);                    /* 11xxxxx */
    for (int i = 0; i < 5; i++) ee_send_bit(0);
    EE_CS_Low(idx);
    ee_delay();

    /* Write 0xA5A5 to addr 127 */
    EE_CS_High(idx);
    ee_delay();
    ee_send_command(0x01, 127);
    uint16_t test_val = 0xA5A5;
    for (int i = 15; i >= 0; i--)
        ee_send_bit((test_val >> i) & 1);
    bool write_ok = ee_wait_ready(idx, 20);
    EE_CS_Low(idx);
    ee_delay();

    if (!write_ok) {
        EE_MuxDisable();
        return false;
    }

    /* Read back addr 127 */
    EE_CS_High(idx);
    ee_delay();
    ee_send_command(0x02, 127);
    ee_recv_bit();  /* dummy */
    uint16_t readback = 0;
    for (int i = 15; i >= 0; i--)
        readback |= ((uint16_t)ee_recv_bit() << i);
    EE_CS_Low(idx);
    ee_delay();

    /* Write disable */
    EE_CS_High(idx);
    ee_delay();
    ee_send_bit(1); ee_send_bit(0); ee_send_bit(0);
    for (int i = 0; i < 7; i++) ee_send_bit(0);
    EE_CS_Low(idx);
    ee_delay();

    EE_MuxDisable();

    return (readback == test_val);
}

/* ── Memory Test ─────────────────────────────────────────────── */
bool EE_TestMemory(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return false;

    EE_WriteEnable(idx);

    /* Pass 1: write 0xAAAA to all addresses, verify */
    for (uint8_t a = 0; a < EE_NUM_ADDRS; a++) {
        if (!EE_Write(idx, a, 0xAAAA))
            goto fail;
    }
    for (uint8_t a = 0; a < EE_NUM_ADDRS; a++) {
        if (EE_Read(idx, a) != 0xAAAA)
            goto fail;
    }

    /* Pass 2: write 0x5555 to all addresses, verify */
    for (uint8_t a = 0; a < EE_NUM_ADDRS; a++) {
        if (!EE_Write(idx, a, 0x5555))
            goto fail;
    }
    for (uint8_t a = 0; a < EE_NUM_ADDRS; a++) {
        if (EE_Read(idx, a) != 0x5555)
            goto fail;
    }

    EE_WriteDisable(idx);
    return true;

fail:
    EE_WriteDisable(idx);
    return false;
}

/* ── Program Zero ────────────────────────────────────────────── */
bool EE_ProgramZero(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return false;

    EE_WriteEnable(idx);

    /* Write 0x0000 to all addresses */
    for (uint8_t a = 0; a < EE_NUM_ADDRS; a++) {
        if (!EE_Write(idx, a, 0x0000))
            goto fail;
    }

    /* Verify */
    for (uint8_t a = 0; a < EE_NUM_ADDRS; a++) {
        if (EE_Read(idx, a) != 0x0000)
            goto fail;
    }

    EE_WriteDisable(idx);
    return true;

fail:
    EE_WriteDisable(idx);
    return false;
}
