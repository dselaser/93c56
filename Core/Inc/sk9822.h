#ifndef SK9822_H
#define SK9822_H

#include "stm32h5xx_hal.h"
#include <stdint.h>

#define SK9822_NUM_LEDS  12

typedef struct {
    uint8_t brightness;  /* 0-31 */
    uint8_t r;
    uint8_t g;
    uint8_t b;
} SK9822_Pixel;

void SK9822_Transmit(SPI_HandleTypeDef *hspi, SK9822_Pixel *pixels, uint8_t count);
void SK9822_Clear(SPI_HandleTypeDef *hspi);

#endif /* SK9822_H */
