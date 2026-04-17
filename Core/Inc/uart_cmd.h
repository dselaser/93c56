#ifndef UART_CMD_H
#define UART_CMD_H

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* UART 명령 태스크에서 사용하는 UART 핸들 */
extern UART_HandleTypeDef huart1;

/* MCP 모드: true 이면 mainAppTask 일시 중지 */
extern volatile bool g_mcp_mode;

/* UART 명령 처리 초기화 (FreeRTOS 태스크 내에서 호출) */
void UART_Cmd_Process(void);

#endif /* UART_CMD_H */
