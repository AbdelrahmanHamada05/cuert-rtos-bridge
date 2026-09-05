
#ifndef FREERTOS_INCLUDE_UART_H_
#define FREERTOS_INCLUDE_UART_H_

#include <stdint.h>
#include "std_types.h"

void UART_init(uint32_t baudrate, uint32_t f_cpu);
void UART_TransmitChar(char c);
void UART_Println(const char *str);
char UART_ReceiveChar(void);
uint8_t UART_Available(void);

#endif /* FREERTOS_INCLUDE_UART_H_ */
