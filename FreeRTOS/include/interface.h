/*
 * interface.h
 *
 *  Created on: Sep 3, 2026
 *      Author: Hp
 */

#ifndef FREERTOS_INCLUDE_INTERFACE_H_
#define FREERTOS_INCLUDE_INTERFACE_H_

#include "uart.h"
#include "actuator.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "std_types.h"

extern volatile TickType_t g_ulLastHeartbeat;

// Expose the global command queue handle to other files
extern QueueHandle_t xCommandQueue;

typedef struct {
	char     type;         // 'T' throttle, 'S' steer, 'B' brake, 'P' ping
	int16_t  value;        // 0..100 (throttle/brake) or -100..100 (steer)
	uint32_t timestamp_ms;
} Command_t;

// Queue and Heartbeat Handle Expositions
extern QueueHandle_t xCommandQueue;
extern volatile TickType_t g_ulLastHeartbeat;

// Global Telemetry Tracking Variables
extern volatile char g_lastCmdType;
extern volatile int16_t g_lastCmdValue;
extern volatile uint8_t g_currentThrottle;
extern volatile int8_t g_currentSteering;
extern volatile uint8_t g_currentBrake;

void Command_Process(const Command_t *cmd);

#endif /* FREERTOS_INCLUDE_INTERFACE_H_ */
