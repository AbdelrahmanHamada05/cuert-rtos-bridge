#include "actuator.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile uint8_t g_duty_cycle = 0;

void Actuator_init(void){
	DDRC |= (1 << PC0) | (1 << PC1) | (1 << PC2);

	TCCR0 = (1 << WGM01) | (1 << CS01); // back to prescaler 8
	OCR0  = 100;                        // Sets interrupt frequency
	TIMSK |= (1 << OCIE0);

}

void Actuator_SetOutput(uint8_t percent) {
    if (percent > 100) percent = 100;
    g_duty_cycle = percent;
}

ISR(TIMER0_COMP_vect){
	static uint8 pwm_counter = 0;

	if (pwm_counter < g_duty_cycle) {
		PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2);  // Drive LEDs HIGH
	} else {
		PORTC &= ~((1 << PC0) | (1 << PC1) | (1 << PC2)); // Drive LEDs LOW
	}

	pwm_counter++;
	if (pwm_counter >= 100) {
		pwm_counter = 0;
	}
}
