#include "interface.h"
#include "std_types.h"
#include <stdio.h>

// Global Telemetry and Watchdog Variable Instantiations (Allocated in SRAM)
volatile TickType_t g_ulLastHeartbeat = 0;
volatile char g_lastCmdType          = 'N'; // 'N' = None (Initial State)
volatile int16_t g_lastCmdValue      = 0;
volatile uint8_t g_currentThrottle   = 0;
volatile int8_t g_currentSteering    = 0;
volatile uint8_t g_currentBrake      = 0;

void Command_Process(const Command_t *cmd) {
    UART_TransmitChar('A');  // entered function

    taskENTER_CRITICAL();
    g_ulLastHeartbeat = xTaskGetTickCount();
    taskEXIT_CRITICAL();
    UART_TransmitChar('B');  // heartbeat updated

    taskENTER_CRITICAL();
    g_lastCmdType = cmd->type;
    g_lastCmdValue = cmd->value;
    taskEXIT_CRITICAL();
    UART_TransmitChar('C');  // telemetry updated

    static char Buffer[64];

    UART_TransmitChar('0');       // about to enter switch
    UART_TransmitChar(cmd->type); // print the actual type byte we're switching on

    // 3. Process actuation command and update output state variables
    switch (cmd->type) {
		case 'T':
			UART_TransmitChar('1');
			if (cmd->value >= 0 && cmd->value <= 100) {
				if(!g_currentBrake){
					g_currentThrottle = (uint8_t)cmd->value;
					Actuator_SetOutput(g_currentThrottle);
					UART_TransmitChar('2'); // about to snprintf
					snprintf(Buffer, sizeof(Buffer), "[%lu ms] Throttle set to %d\r\n",
							 (unsigned long)cmd->timestamp_ms, cmd->value);
					UART_TransmitChar('3'); // snprintf survived
					UART_Println(Buffer);
					UART_TransmitChar('4'); // println survived
				} else {
					UART_Println("Throttle Ignored");
				}
			}
			break;

        case 'S': // Steering (-100 to +100)
            if (cmd->value >= -100 && cmd->value <= 100) {
                g_currentSteering = (int8_t)cmd->value;

                // Scale steering range (-100..100) to duty cycle (0..100%) for LED testing
                uint8_t steerDuty = (uint8_t)(((int16_t)cmd->value + 100) / 2);
                Actuator_SetOutput(steerDuty);

                snprintf(Buffer, sizeof(Buffer), "[%lu ms] Steer set to %d\r\n",
                         (unsigned long)cmd->timestamp_ms, cmd->value);
                UART_Println(Buffer);
            }
            break;

        case 'B': // Emergency Brake (0 to 100%)
            if (cmd->value >= 0 && cmd->value <= 100) {
                g_currentBrake = (uint8_t)cmd->value;
                if (g_currentBrake > 0) {
                    g_currentThrottle = 0; // Clear throttle state on braking
                }

                Actuator_SetOutput(g_currentBrake);

                snprintf(Buffer, sizeof(Buffer), "[%lu ms] Brake set to %d\r\n",
                         (unsigned long)cmd->timestamp_ms, cmd->value);
                UART_Println(Buffer);
            }
            break;

        case 'P': // Heartbeat Ping
            snprintf(Buffer, sizeof(Buffer), "[%lu ms] Ping received\r\n",
                     (unsigned long)cmd->timestamp_ms);
            UART_Println(Buffer);
            break;

        default:
            UART_TransmitChar('9');
            UART_Println("[ERR] Unknown command type\r\n");
            break;
    }
    UART_TransmitChar('Z'); // exited switch cleanly
}
