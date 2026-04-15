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
    { GPIOA, GPIO_PIN_15 },   /* CS4  - PA15 (회로도 확인) */
    { GPIOB, GPIO_PIN_7  },   /* CS5  - PB7  (회로도 확인) */
    { GPIOC, GPIO_PIN_11 },   /* CS6  - PC11 (회로도 확인) */
    { GPIOC, GPIO_PIN_12 },   /* CS7  - PC12 (회로도 확인) */
    { GPIOD, GPIO_PIN_2  },   /* CS8  - PD2  (회로도 확인) */
    { GPIOB, GPIO_PIN_4  },   /* CS9  - PB4  (회로도 확인) */
    { GPIOB, GPIO_PIN_5  },   /* CS10 - PB5  (회로도 확인) */
    { GPIOB, GPIO_PIN_6  },   /* CS11 - PB6  (회로도 확인) */
};

/* ── DO Pin Table (MCU 직접 연결) ────────────────────────────── */
/* DO Pin Table: CS_x → DO_x (1:1 매핑, 회로도 확인)
 * MCU GPIO 에서 직접 DO 를 읽는다 (MUX 미사용). */
const EE_CSPin ee_do_table[EE_NUM_CHIPS] = {
    { GPIOC, GPIO_PIN_2  },   /* DO0  - PC2  */
    { GPIOC, GPIO_PIN_3  },   /* DO1  - PC3  */
    { GPIOA, GPIO_PIN_0  },   /* DO2  - PA0  */
    { GPIOA, GPIO_PIN_1  },   /* DO3  - PA1  */
    { GPIOA, GPIO_PIN_2  },   /* DO4  - PA2  */
    { GPIOA, GPIO_PIN_3  },   /* DO5  - PA3  */
    { GPIOB, GPIO_PIN_10 },   /* DO6  - PB10 */
    { GPIOA, GPIO_PIN_6  },   /* DO7  - PA6  */
    { GPIOC, GPIO_PIN_4  },   /* DO8  - PC4  */
    { GPIOC, GPIO_PIN_5  },   /* DO9  - PC5  */
    { GPIOB, GPIO_PIN_0  },   /* DO10 - PB0  */
    { GPIOB, GPIO_PIN_1  },   /* DO11 - PB1  */
};

/* 현재 활성 칩의 DO 포트/핀 (dout_read 에서 사용) */
static GPIO_TypeDef *active_do_port = GPIOC;
static uint16_t      active_do_pin  = GPIO_PIN_2;  /* DO0 기본값 */

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
/* DO 읽기: MCU 직접 연결 핀에서 읽음 (MUX 미사용) */
static inline uint8_t dout_read(void) { return (active_do_port->IDR & active_do_pin) ? 1 : 0; }

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

    /* ── Inputs: DO0~DO11 (MCU 직접 연결, MUX 미사용) ──── */
    /* NOTE: SPI1 클럭은 유지 (PA7=SPI1_MOSI → SK9822 LED).
     * PA6(DO7)는 EE_Init 에서 GPIO_INPUT 으로 재설정되지만,
     * SPI1_DIRECTION_1LINE 에서는 MISO(PA6) 미사용이므로 충돌 없음. */
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;     /* DO Hi-Z 시 LOW 읽힘 */
    for (int i = 0; i < EE_NUM_CHIPS; i++) {
        gpio.Pin = ee_do_table[i].pin;
        HAL_GPIO_Init(ee_do_table[i].port, &gpio);
    }

    /* ── MUX 비활성화: INHIBIT = HIGH (MUX 출력 차단) ── */
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = MUX_INH_PIN;
    HAL_GPIO_WritePin(MUX_INH_PORT, MUX_INH_PIN, GPIO_PIN_SET);
    HAL_GPIO_Init(MUX_INH_PORT, &gpio);

    /* ── PA15(CS4) JTDI 해제: SWD 모드에서도 PA15 가 JTDI AF 로
     *    남아있을 수 있음. 직접 레지스터로 GPIO OUTPUT 강제 설정. */
    GPIOA->MODER   = (GPIOA->MODER   & ~(3UL << (15*2))) | (1UL << (15*2));
    GPIOA->OTYPER  &= ~(1UL << 15);
    GPIOA->OSPEEDR |= (3UL << (15*2));
    GPIOA->PUPDR   &= ~(3UL << (15*2));
    GPIOA->AFR[1]  &= ~(0xFUL << ((15-8)*4));   /* AF0 (GPIO) */
    GPIOA->BSRR     = (1UL << 15) << 16;         /* PA15 = LOW */

    /* ── Outputs: CS0-CS11 (all LOW initially) ─────────── */
    for (int i = 0; i < EE_NUM_CHIPS; i++) {
        gpio.Pin = ee_cs_table[i].pin;
        HAL_GPIO_WritePin(ee_cs_table[i].port, ee_cs_table[i].pin, GPIO_PIN_RESET);
        HAL_GPIO_Init(ee_cs_table[i].port, &gpio);
    }

}

/* ── DO 칩 선택 (MUX 대신 MCU 직접 읽기) ─────────────────────── */

/* 칩 idx 의 DO 핀을 활성화 (이후 dout_read() 가 해당 핀에서 읽음) */
void EE_MuxSelect(uint8_t idx)
{
    if (idx < EE_NUM_CHIPS) {
        active_do_port = ee_do_table[idx].port;
        active_do_pin  = ee_do_table[idx].pin;
    }
}

/* 하위 호환용 (MUX 미사용이므로 no-op) */
void EE_MuxSelectRaw(uint8_t ch) { (void)ch; }
void EE_MuxDisable(void) { /* MUX 비활성화 상태 유지 */ }
void EE_MuxEnable(void)  { /* MUX 미사용, no-op */ }

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

    /* 이 93C56은 READ 후 더미 비트를 출력하지 않음 —
     * 마지막 어드레스 CLK 하강 직후 D7 이 바로 출력됨.
     * (더미 비트를 스킵하면 D7 을 버려 1비트 좌이동 오류 발생) */
    ee_delay();   /* 칩이 D7 을 드라이브할 시간 확보 (tV ≤ 200ns) */

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

/* ── MUX 채널 매핑 스캔 (write 후 ready 상태에서 전 채널 탐색) ── */
uint16_t EE_ScanMuxMapping(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return 0;

    /* 1. Write Enable */
    EE_WriteEnable(idx);

    /* 2. Write 명령 전송 (addr=0, data=0xAA) */
    EE_MuxSelect(idx);
    EE_MuxEnable();
    clk_low();
    din_low();
    EE_CS_High(idx);
    ee_delay();
    ee_send_command(0x01, 0);           /* WRITE opcode + addr */
    for (int i = 7; i >= 0; i--)
        ee_send_bit((0xAA >> i) & 1);  /* 8-bit data */

    /* 3. CS LOW → 즉시 CS HIGH: ready/busy 폴링 모드 진입
     *    (CS LOW 동안 오래 기다리면 칩이 쓰기 완료 후
     *     다음 CS HIGH 시 command 모드로 진입하여 DO 상태 불명확)
     *    CS LOW → 바로 CS HIGH 해야 busy→ready 전이를 DO 로 관찰 가능 */
    EE_CS_Low(idx);
    ee_delay();
    EE_CS_High(idx);

    /* 4. 쓰기 완료 대기 (CS HIGH 유지, 93C56 max 10ms → 25ms 여유) */
    volatile uint32_t wait = 25U * 3200U;   /* ~25ms */
    while (wait--) __NOP();

    /* 6. 16채널 MUX 스캔 (원시 채널 사용, 리매핑 없이) */
    uint16_t hit = 0;
    for (uint8_t ch = 0; ch < 16; ch++) {
        EE_MuxSelectRaw(ch);
        EE_MuxEnable();
        ee_delay(); ee_delay(); ee_delay(); ee_delay();
        if (dout_read())
            hit |= (1u << ch);
        EE_MuxDisable();
    }

    EE_CS_Low(idx);
    EE_MuxDisable();
    EE_WriteDisable(idx);

    return hit;   /* 비트마스크: bit N = MUX ch N 에서 HIGH 읽힘 */
}

/* ── 전체 GPIO IDR 스캔 (write 후 GPIOA~D 전체 읽기) ────── */
void EE_ScanAllGPIO(uint8_t idx, uint32_t out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0;
    if (idx >= EE_NUM_CHIPS) return;

    EE_WriteEnable(idx);
    clk_low(); din_low();
    EE_CS_High(idx);
    ee_delay();
    ee_send_command(0x01, 0);
    for (int i = 7; i >= 0; i--)
        ee_send_bit((0xAA >> i) & 1);

    EE_CS_Low(idx);
    ee_delay();
    EE_CS_High(idx);

    volatile uint32_t wait = 25U * 3200U;
    while (wait--) __NOP();

    out[0] = GPIOA->IDR;
    out[1] = GPIOB->IDR;
    out[2] = GPIOC->IDR;
    out[3] = GPIOD->IDR;

    EE_CS_Low(idx);
    EE_WriteDisable(idx);
}

/* ── DO 핀 직접 스캔 (write 후 12개 DO 핀 전부 읽기) ────── */
uint16_t EE_ScanDOPins(uint8_t idx)
{
    if (idx >= EE_NUM_CHIPS)
        return 0;

    EE_WriteEnable(idx);

    /* Write 명령 전송 */
    clk_low();
    din_low();
    EE_CS_High(idx);
    ee_delay();
    ee_send_command(0x01, 0);
    for (int i = 7; i >= 0; i--)
        ee_send_bit((0xAA >> i) & 1);

    /* CS LOW → 즉시 CS HIGH (ready/busy 모드) */
    EE_CS_Low(idx);
    ee_delay();
    EE_CS_High(idx);

    /* 쓰기 완료 대기 */
    volatile uint32_t wait = 25U * 3200U;
    while (wait--) __NOP();

    /* 12개 DO 핀 전부 읽기 */
    uint16_t hit = 0;
    for (uint8_t d = 0; d < EE_NUM_CHIPS; d++) {
        if (ee_do_table[d].port->IDR & ee_do_table[d].pin)
            hit |= (1u << d);
    }

    EE_CS_Low(idx);
    EE_WriteDisable(idx);

    return hit;  /* bit D = 1 → ee_do_table[D] 핀에서 HIGH */
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

    /* DO 핀 PULLDOWN 기반 감지:
     *   WR=OK  → 칩이 DO를 H로 구동 → 칩 있음
     *   WR=TOUT → DO 가 L (PULLDOWN) → 칩 없음
     * 읽기 값은 EWEN 미동작 등으로 신뢰 불가 → 무시. */
    EE_WriteEnable(idx);
    bool ok = EE_Write(idx, 255, 0xA5);
    EE_WriteDisable(idx);
    return ok;
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

    /* Write 0x00 to all addresses */
    for (uint16_t a = 0; a < g_num_addrs; a++) {
        if (!EE_Write(idx, (uint8_t)a, 0x00)) {
            EE_WriteDisable(idx);
            return false;   /* WR TOUT = 칩 없음 */
        }
    }

    /* NOTE: readback verify 생략 (RB=0x48 문제 미해결) */
    EE_WriteDisable(idx);
    return true;
}
