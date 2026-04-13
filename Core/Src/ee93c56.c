/**
 * ee93c56.c — 93C46/93C56 Microwire EEPROM bit-bang driver
 *
 * 93C46 x8: 128 x 8-bit, 7-bit addr  (1Kbit)
 * 93C56 x8: 256 x 8-bit, 8-bit addr  (2Kbit)
 *
 * Microwire command format (x8):
 *   START_BIT(1) + OPCODE(2) + ADDRESS(7 or 8) [+ DATA(8)]
 */

#include "ee93c56.h"

/* ── 칩 종류 전역 변수 (기본값: 93C56) ───────────────────────── */
volatile EE_ChipType g_chip_type = EE_CHIP_UNKNOWN;
volatile uint8_t     g_addr_bits = 8;     /* 93C56 기본 */
volatile uint16_t    g_num_addrs = 256;   /* 93C56 기본 */

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

/* Send start bit + opcode(2) + address(g_addr_bits) */
static void ee_send_command(uint8_t opcode, uint8_t addr)
{
    ee_send_bit(1);  /* Start bit */
    ee_send_bit((opcode >> 1) & 1);  /* Opcode MSB */
    ee_send_bit(opcode & 1);         /* Opcode LSB */
    /* Address: g_addr_bits (7 for 93C46, 8 for 93C56) */
    for (int i = g_addr_bits - 1; i >= 0; i--)
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

    /* EWEN: SB(1) + 00 + 11 + (addr_bits-2)개 0 */
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(0);
    ee_send_bit(1);
    ee_send_bit(1);
    for (int i = 0; i < (int)g_addr_bits - 2; i++)
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

    /* EWDS: SB(1) + 00 + addr_bits개 0 */
    ee_send_bit(1);
    ee_send_bit(0);
    ee_send_bit(0);
    for (int i = 0; i < (int)g_addr_bits; i++)
        ee_send_bit(0);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();
}

/* ── Read (x8: 8-bit data) ───────────────────────────────────── */
uint8_t EE_Read(uint8_t idx, uint8_t addr)
{
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* READ: opcode = 10 */
    ee_send_command(0x02, addr);

    /* Dummy bit (leading zero before data) */
    ee_recv_bit();

    /* Read 8-bit data MSB first */
    uint8_t data = 0;
    for (int i = 7; i >= 0; i--)
        data |= (ee_recv_bit() << i);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();

    return data;
}

/* ── Write (x8: 8-bit data) ──────────────────────────────────── */
bool EE_Write(uint8_t idx, uint8_t addr, uint8_t data)
{
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();

    EE_CS_High(idx);
    ee_delay();

    /* WRITE: opcode = 01 */
    ee_send_command(0x01, addr);

    /* Send 8-bit data MSB first */
    for (int i = 7; i >= 0; i--)
        ee_send_bit((data >> i) & 1);

    /* Wait for write completion */
    bool ok = ee_wait_ready(idx, 20);

    EE_CS_Low(idx);
    ee_delay();
    EE_MuxDisable();

    return ok;
}

/* ── 읽기 전용 감지 (쓰기 없음) ─────────────────────────── */
bool EE_DetectReadOnly(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return false;

    /* 주소 0 과 64 를 읽어서 비교.
     * 칩 없음(풀다운) → DOUT 항상 LOW → 읽기 결과 0x00
     * 칩 있음 → 비-제로 데이터면 확정; 둘 다 0x00이면 재확인.
     * ※ 전체가 0x00으로 프로그래밍된 칩은 감지 불가(한계).
     *   프로그래밍 완료 칩 감지에는 EE_Detect() 사용 권장. */

    uint8_t val0  = EE_Read(idx, 0);
    uint8_t val64 = EE_Read(idx, 64);

    if (val0 != 0x00 || val64 != 0x00)
        return true;

    /* 둘 다 0x00 → 재확인 */
    uint8_t val0b  = EE_Read(idx, 0);
    uint8_t val64b = EE_Read(idx, 64);

    if (val0b == 0x00 && val64b == 0x00)
        return false;  /* 칩 없거나 전부 0x00 */

    return true;
}

/* ── Detect (쓰기 포함, 확정 감지, x8) ──────────────────────── */
bool EE_Detect(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return false;

    /* Write enable → write 0xA5 to addr 255 → read back → verify */
    EE_WriteEnable(idx);

    uint8_t test_val = 0xA5;
    if (!EE_Write(idx, 255, test_val)) {
        EE_WriteDisable(idx);
        return false;
    }

    uint8_t readback = EE_Read(idx, 255);

    EE_WriteDisable(idx);

    return (readback == test_val);
}

/* ── 칩 종류 식별 (93C46 vs 93C56) ───────────────────────────── */
EE_ChipType EE_IdentifyChip(void)
{
    /* 첫 번째 감지된 칩(idx=0 부터)으로 판별.
     * 방법: 93C56 모드(8-bit addr)로 주소 200에 쓰기/읽기 시도.
     *       93C46은 주소 127까지만 → 주소 200은 존재하지 않아 실패.
     *       93C56이면 성공. */

    /* 먼저 감지된 칩 찾기 */
    uint8_t test_idx = 255;
    for (uint8_t i = 0; i < EE_NUM_CHIPS; i++) {
        /* 간단 읽기로 칩 존재 확인 */
        uint8_t val = EE_Read(i, 0);
        /* 아무 칩이든 시도 */
        test_idx = i;
        break;
    }
    if (test_idx == 255) {
        g_chip_type = EE_CHIP_UNKNOWN;
        return EE_CHIP_UNKNOWN;
    }

    /* ── 93C56 모드로 시도 (8-bit addr) ────────────── */
    g_addr_bits = 8;
    g_num_addrs = 256;

    EE_WriteEnable(test_idx);

    /* 주소 200에 0xA5 쓰기 (93C46 범위 밖) */
    bool w_ok = EE_Write(test_idx, 200, 0xA5);
    uint8_t rb = 0;
    if (w_ok)
        rb = EE_Read(test_idx, 200);

    /* 원래 값 복원 (나중에 프로그래밍에서 0으로 됨) */
    EE_WriteDisable(test_idx);

    if (w_ok && rb == 0xA5) {
        /* 93C56 확정 */
        g_chip_type = EE_CHIP_93C56;
        g_addr_bits = 8;
        g_num_addrs = 256;
        return EE_CHIP_93C56;
    }

    /* ── 93C46 모드로 전환 (7-bit addr) ────────────── */
    g_addr_bits = 7;
    g_num_addrs = 128;

    /* 주소 100에 0x5A 쓰기/읽기 (93C46 범위 내) */
    EE_WriteEnable(test_idx);
    w_ok = EE_Write(test_idx, 100, 0x5A);
    rb = 0;
    if (w_ok)
        rb = EE_Read(test_idx, 100);
    EE_WriteDisable(test_idx);

    if (w_ok && rb == 0x5A) {
        g_chip_type = EE_CHIP_93C46;
        return EE_CHIP_93C46;
    }

    /* 둘 다 실패 */
    g_chip_type = EE_CHIP_UNKNOWN;
    g_addr_bits = 8;
    g_num_addrs = 256;
    return EE_CHIP_UNKNOWN;
}

/* ── Memory Test ─────────────────────────────────────────────── */
bool EE_TestMemory(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return false;

    EE_WriteEnable(idx);

    /* Pass 1: write 0xAA to all 256 addresses, verify */
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (!EE_Write(idx, (uint8_t)a, 0xAA))
            goto fail;
    }
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (EE_Read(idx, (uint8_t)a) != 0xAA)
            goto fail;
    }

    /* Pass 2: write 0x55 to all 256 addresses, verify */
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (!EE_Write(idx, (uint8_t)a, 0x55))
            goto fail;
    }
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (EE_Read(idx, (uint8_t)a) != 0x55)
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

    /* Write 0x00 to all 256 addresses */
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (!EE_Write(idx, (uint8_t)a, 0x00))
            goto fail;
    }

    /* Verify */
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (EE_Read(idx, (uint8_t)a) != 0x00)
            goto fail;
    }

    EE_WriteDisable(idx);
    return true;

fail:
    EE_WriteDisable(idx);
    return false;
}
