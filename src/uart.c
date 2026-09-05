#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define RX_BUFFER_SIZE 32

static volatile char rxBuffer[RX_BUFFER_SIZE];
static volatile uint8_t rxHead = 0; // next write position
static volatile uint8_t rxTail = 0; // next read position

void UART_init(uint32_t baudrate, uint32_t f_cpu){
	uint16 ubrr = (uint16)((f_cpu/(16UL * baudrate))-1);

	UBRRH = (uint8_t)(ubrr >> 8);
	UBRRL = (uint8_t)ubrr;

	// Enable Receiver, Transmitter, AND RX Complete Interrupt
	UCSRB = (1 << RXEN) | (1 << TXEN) | (1 << RXCIE);

	UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

// Fires the instant a byte arrives, regardless of what any task is doing
ISR(USART_RXC_vect){
	char c = UDR; // must read UDR to clear the interrupt flag
	uint8_t nextHead = (rxHead + 1) % RX_BUFFER_SIZE;
	if (nextHead != rxTail) { // only store if buffer isn't full
		rxBuffer[rxHead] = c;
		rxHead = nextHead;
	}
	// if full, byte is dropped here instead of at the hardware level —
	// at 32 bytes deep this should never happen for your use case
}

void UART_TransmitChar(char c){
	while (!(UCSRA & (1 << UDRE)));
	UDR = c;
}

void UART_Println(const char *str) {
    while (*str) {
        UART_TransmitChar(*str++);
    }
    UART_TransmitChar('\r');
    UART_TransmitChar('\n');
}

char UART_ReceiveChar(void) {
    while (rxHead == rxTail); // wait if buffer is empty (shouldn't happen if caller checks Available first)
    char c = rxBuffer[rxTail];
    rxTail = (rxTail + 1) % RX_BUFFER_SIZE;
    return c;
}

uint8_t UART_Available(void) {
    return (rxHead != rxTail) ? 1 : 0;
}
