#include <avr/io.h>
#include "std_types.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "FreeRTOSConfig.h"
#include "interface.h"

#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xFFFF)
#endif

QueueHandle_t xCommandQueue = NULL;

#define MAX_BUFFER_SIZE  64

void vCommandRxTask(void *pvParameters);
//void vTestConsumerTask(void *pvParameters);
void vActuateTask(void *pvParameters);
void vWatchdogTask(void *pvParameters);
void vStatusTask(void *pvParameters);

int main(void) {
	UART_init(9600, F_CPU);
	Actuator_init();

	UART_Println("MCU BOOT OK\r\n");

	xCommandQueue = xQueueCreate(5, sizeof(Command_t));
	if (xCommandQueue == NULL) {
	    UART_Println("QUEUE CREATE FAILED - OUT OF HEAP");
	    while(1); // halt here so you know immediately
	}

	BaseType_t xResult;

	xResult = xTaskCreate(vWatchdogTask, "WDT", 120, NULL, 2, NULL);
	if (xResult != pdPASS) {
	    UART_Println("WDT TASK CREATE FAILED - OUT OF HEAP");
	    while(1);
	}

	xResult = xTaskCreate(vCommandRxTask, "CMD_RX", 220, NULL, 4, NULL);
	if (xResult != pdPASS) {
	    UART_Println("CMD_RX TASK CREATE FAILED - OUT OF HEAP");
	    while(1);
	}

	xResult = xTaskCreate(vActuateTask, "ACTUATE", 150, NULL, 3, NULL);
	if (xResult != pdPASS) {
	    UART_Println("ACTUATE TASK CREATE FAILED - OUT OF HEAP");
	    while(1);
	}

	xResult = xTaskCreate(vStatusTask, "STATUS", 250, NULL, 1, NULL);
	if (xResult != pdPASS) {
	    UART_Println("STATUS TASK CREATE FAILED - OUT OF HEAP");
	    while(1);
	}


//	xTaskCreate(vTestConsumerTask, "TEST_RX", 200, NULL, 3, NULL);
//	xTaskCreate(vCommandRxTask,    "CMD_RX",  220, NULL, 2, NULL);

	vTaskStartScheduler();

	while(1);
    return 0;
}



void vCommandRxTask(void *pvParameters){
	(void)pvParameters;
	char Buffer[MAX_BUFFER_SIZE];
	Command_t cmd;
	char c;
	sint32 i = 0;
	int val = 0;
	while(1){
		while (!UART_Available()) { vTaskDelay(pdMS_TO_TICKS(1)); }
		c = UART_ReceiveChar();
		if(c == '\r' || c == '\n'){
			if(i > 0){
				Buffer[i] = '\0';
				uint8 validCommand = 0;
				if(sscanf(Buffer, "THROTTLE %d", &val) == 1){
					if (val >= 0 && val <= 100) {
						cmd.type = 'T';
						cmd.value = (int16_t)val;
						validCommand = 1;
					}
				}
				else if(sscanf(Buffer, "STEER %d", &val) == 1){
					if (val >= -100 && val <= 100) {
						cmd.type = 'S';
						cmd.value = (int16_t)val;
						validCommand = 1;
					}
				}
				else if(sscanf(Buffer, "BRAKE %d", &val) == 1){
					if (val >= 0 && val <= 100) {
						cmd.type = 'B';
						cmd.value = (int16_t)val;
						validCommand = 1;
					}
				}
				else if(strcmp(Buffer, "PING") == 0){
					cmd.type = 'P';
					cmd.value = 0;
					validCommand = 1;
				}
				if(validCommand){
					cmd.timestamp_ms = (xTaskGetTickCount() * 1000/configTICK_RATE_HZ);
					if(cmd.type == 'B'){
						if (xQueueSendToFront(xCommandQueue, &cmd, 0) != pdPASS) {
							Command_t temp;
							xQueueReceive(xCommandQueue, &temp, 0); // Drop oldest item
							xQueueSendToFront(xCommandQueue, &cmd, 0);
						}
					} else {
                        // Standard Priority: Send to back
                        xQueueSendToBack(xCommandQueue, &cmd, pdMS_TO_TICKS(10));
                    }
				}
			}
			i = 0;
		}
		else if (c == '\b' || c == 0x7F) {
			if (i > 0) i--;
		}
		else if (i < sizeof(Buffer) - 1) {
			Buffer[i++] = c;
		}
	}
}

//void vTestConsumerTask(void *pvParameters) {
//    Command_t cmd;
//    char debugBuffer[64];
//
//    while (1) {
//        // Block until vCommandRxTask puts a Command_t into xCommandQueue
//        if (xQueueReceive(xCommandQueue, &cmd, portMAX_DELAY) == pdPASS) {
//            // Echo parsed struct contents back over UART
//            snprintf(debugBuffer, sizeof(debugBuffer),
//                     "\r\n[RX TEST] Type: %c | Value: %d | Time: %lu ms\r\n",
//                     cmd.type, cmd.value, (unsigned long)cmd.timestamp_ms);
//
//            UART_Println(debugBuffer);
//        }
//    }
//}

void vActuateTask(void *pvParameters){
	(void)pvParameters;
	Command_t cmd;
	while(1){
		if (xQueueReceive(xCommandQueue, &cmd, portMAX_DELAY) == pdPASS){
			Command_Process(&cmd);
		}
	}
}
void vWatchdogTask(void *pvParameters){
	(void)pvParameters;
	static uint8_t isFailsafeActive = 0;
	static uint8_t blinkState = 0;
    taskENTER_CRITICAL();
    g_ulLastHeartbeat = xTaskGetTickCount();
    taskEXIT_CRITICAL();
	while(1){
		TickType_t xCurrentTick = xTaskGetTickCount();
		TickType_t xElapsedTicks = xCurrentTick - g_ulLastHeartbeat;
		if (xElapsedTicks > pdMS_TO_TICKS(500)){
			if(!isFailsafeActive){

				isFailsafeActive = 1;

//				Actuator_Brake(100);
				if (xCommandQueue != NULL) {
					xQueueReset(xCommandQueue);
				}

				UART_Println("LINK LOST , failing safe\r\n");
			}
			blinkState = !blinkState;
			if (blinkState) {
				Actuator_SetOutput(100);
			} else {
				Actuator_SetOutput(0);
			}
		}
		else{
			if(isFailsafeActive){
				isFailsafeActive = 0;
				UART_Println("[FAILSAFE] Link restored. Resuming operation.\r\n");
			}
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void vStatusTask(void *pvParameters){
	(void)pvParameters;
	static char statusBuffer[100];

	TickType_t xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(1000);
	while(1){
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		uint32_t uptime_seconds = (uint32_t)xTaskGetTickCount() / configTICK_RATE_HZ;

		snprintf(statusBuffer, sizeof(statusBuffer),
				 "[STATUS @ %lus] Last: %c %d | T: %u%% | S: %d | B: %u%%\r\n",
				 (unsigned long)uptime_seconds,
				 g_lastCmdType,
				 g_lastCmdValue,
				 g_currentThrottle,
				 g_currentSteering,
				 g_currentBrake);

		UART_Println(statusBuffer);
	}
}
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    UART_Println("!!! STACK OVERFLOW in task:");
    UART_Println(pcTaskName);
    while(1); // halt so the message stays visible and you know exactly where it happened
}
