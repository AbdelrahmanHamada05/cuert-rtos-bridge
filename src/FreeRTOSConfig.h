#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <avr/io.h>

#define configCHECK_FOR_STACK_OVERFLOW        2
/* Hardware specifications for Eta32mini (ATmega32) */
#define configCPU_CLOCK_HZ                    ( ( uint32_t ) 16000000UL )
#define configTICK_RATE_HZ                    ( ( TickType_t ) 1000 )

/* Kernel configuration (Optimized for 2 KB SRAM) */
#define configUSE_PREEMPTION                  1
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configMAX_PRIORITIES                  ( 5 )
#define configMINIMAL_STACK_SIZE              (  ( unsigned short ) 85 )
#define configTOTAL_HEAP_SIZE                 ( ( size_t ) ( 1100 ) )
#define configMAX_TASK_NAME_LEN               ( 8 )
#define configUSE_16_BIT_TICKS                1
#define configIDLE_SHOULD_YIELD               1
#define configUSE_MUTEXES                     1
#define configUSE_COUNTING_SEMAPHORES         1

/* Feature exclusions to minimize Flash/RAM usage */
#define configUSE_CO_ROUTINES                 0
#define configMAX_CO_ROUTINE_PRIORITIES       ( 2 )
#define configUSE_TIMERS                      0

/* API function inclusions */
#define INCLUDE_vTaskPrioritySet              1
#define INCLUDE_uxTaskPriorityGet             1
#define INCLUDE_vTaskDelete                   1
#define INCLUDE_vTaskCleanUpResources         0
#define INCLUDE_vTaskSuspend                  1
#define INCLUDE_vTaskDelayUntil               1
#define INCLUDE_vTaskDelay                    1

#endif /* FREERTOS_CONFIG_H */
