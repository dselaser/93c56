#include "sk9822.h"
#include <string.h>

/*
 * SK9822 SPI 프로토콜 (APA102 호환)
 *
 * 프레임 구조 (전체를 단일 HAL_SPI_Transmit으로 전송):
 *   Start  : 4 bytes  = 0x00 0x00 0x00 0x00
 *   Per LED: 4 bytes  = [111BBBBB][Blue][Green][Red]
 *   End    : 4 bytes  = 0xFF 0xFF 0xFF 0xFF
 */

#define FRAME_SIZE  (4 + SK9822_NUM_LEDS * 4 + 4)

void SK9822_Transmit(SPI_HandleTypeDef *hspi, SK9822_Pixel *pixels, uint8_t count)
{
    uint8_t frame[FRAME_SIZE];
    uint8_t idx = 0;

    /* Start frame */
    frame[idx++] = 0x00;
    frame[idx++] = 0x00;
    frame[idx++] = 0x00;
    frame[idx++] = 0x00;

    /* LED frames */
    for (uint8_t i = 0; i < count; i++) {
        frame[idx++] = 0xE0 | (pixels[i].brightness & 0x1F);
        frame[idx++] = pixels[i].b;
        frame[idx++] = pixels[i].g;
        frame[idx++] = pixels[i].r;
    }

    /* End frame */
    frame[idx++] = 0xFF;
    frame[idx++] = 0xFF;
    frame[idx++] = 0xFF;
    frame[idx++] = 0xFF;

    HAL_SPI_Transmit(hspi, frame, idx, HAL_MAX_DELAY);
}

void SK9822_Clear(SPI_HandleTypeDef *hspi)
{
    SK9822_Pixel off[SK9822_NUM_LEDS];
    memset(off, 0, sizeof(off));
    SK9822_Transmit(hspi, off, SK9822_NUM_LEDS);
}
