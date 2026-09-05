
#ifndef FREERTOS_INCLUDE_ACTUATOR_H_
#define FREERTOS_INCLUDE_ACTUATOR_H_

#include <stdint.h>
#include "std_types.h"



void Actuator_init(void);
void Actuator_SetOutput(uint8_t percent);
//void Actuator_SetThrottle(uint8_t percent);
//void Actuator_SetSteering(int8_t percent);
//void Actuator_Brake(uint8_t percent);
//void Actuator_KeepAlive(void);

#endif /* FREERTOS_INCLUDE_ACTUATOR_H_ */
