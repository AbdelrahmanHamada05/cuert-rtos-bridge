# CUERT Pre-Interview Task — RTOS Command-to-Actuator Bridge

**Board used:** ATmega32 (AVR)
**Toolchain:** Eclipse + AVR-GCC (via the AVR Eclipse plugin) — *confirm/correct this if it's not exactly right*
**Serial terminal:** Tera Term
**Baud rate:** **9600** — note this differs from the 115200 default mentioned in the task brief; the firmware initializes UART at 9600 (`UART_init(9600, F_CPU)`), so the terminal must be set to match.

> The task's suggested/recommended boards are ESP32, RP2040, or AVR Uno/Nano. I used an ATmega32 because it's the board I had available. It's an older, more resource-constrained AVR chip (2 KB SRAM total), which shaped several of the decisions and bugs described below.

---

## 1. Hardware Setup

- **MCU:** ATmega32, running at 16 MHz (external crystal — confirm fuse bits are set for external oscillator, not internal RC, or all timing will be wrong).
- **Output:** Onboard/wired LED(s) on **PC0, PC1, PC2** (driven together as one PWM output, standing in for the real actuator's motor drive signal).
- **Input:** USB-serial connection (UART), no external sensors, no potentiometers — matches the task's "just microcontroller and one LED and wiring" requirement.
- **Wiring:** [fill in — describe your exact LED/resistor wiring to PC0–PC2, and which USB-serial adapter/programmer you used]

---

## 2. How to Build and Flash

1. Open the project in Eclipse IDE.
2. Build the project (`Build All` / `make`).
3. Flash via USBasp to the ATmega32.
4. Confirm fuse bits select the external 16 MHz crystal (`CKSEL`/`SUT` fuses) — a fuse mismatch will make all UART and RTOS timing wrong even though the code itself is correct.

---

## 3. How to Test (Serial Monitor)

1. Open Tera Term.
2. **Setup → Serial Port:** select the correct COM port, baud rate **9600**, 8 data bits, no parity, 1 stop bit, no flow control.
3. **Setup → Terminal:** set new-line (transmit) to send a recognizable line ending; enable local echo so you can see what you type.
4. Reset the board. You should see `MCU BOOT OK` print once.
5. Type each command on its own line and press Enter:

   | Command | Effect |
   |---|---|
   | `PING` | Acknowledges — proves UART + COMMAND_RX are alive. |
   | `THROTTLE <0-100>` | Sets LED brightness (PWM duty) to that percent, unless brake is active. |
   | `STEER <-100..100>` | Records/logs steering value; output is not driven while brake is active (see §6, item 10). |
   | `BRAKE <0-100>` | Immediately overrides output; any value >0 forces output to 0 and **locks out throttle and steer** until `BRAKE 0` is sent. |

6. While idle: if no valid command arrives for 500 ms, the board logs `LINK LOST , failing safe` and the LED switches to a distinct blink pattern (alternating fully on/off every 100 ms). Sending any valid command immediately logs `[FAILSAFE] Link restored. Resuming operation.` and resumes normal output.
7. `STATUS` prints once per second in the background the whole time, showing uptime, last command, and current throttle/steer/brake state — this runs continuously and independently of whatever else you're testing.

---

## 4. System Design — Four RTOS Tasks

| Task | Priority | Responsibility |
|---|---|---|
| **COMMAND_RX** | 4 (highest) | Reads UART bytes (interrupt-driven, see §6), parses lines into `Command_t`, pushes onto the queue. BRAKE is always sent to the **front** of the queue (and never dropped, even under load — an oldest item is evicted to make room if the queue is full) so it's always processed before anything else waiting. |
| **ACTUATE** | 3 | Pulls the queue, calls `Command_Process()`, which updates output state and drives the PWM via `Actuator_SetOutput()`. |
| **WATCHDOG** | 2 | Tracks time since the last valid command. Forces the fail-safe blink and logs `LINK LOST` after 500 ms of silence; clears automatically the instant a new command arrives. |
| **STATUS** | 1 (lowest) | Prints a status line once per second — the least time-critical task, and the one that can tolerate the most scheduling jitter. |

**Why this order:** COMMAND_RX is the only link to the outside world — the whole system's job is worthless if it isn't scheduled responsively enough to keep up with (and never drop) incoming bytes, especially BRAKE. ACTUATE needs to react promptly to whatever COMMAND_RX has queued. WATCHDOG only needs to notice staleness within a tolerance (a bit of scheduling jitter on a 500 ms check doesn't matter), so it can wait behind more time-critical work — its priority reflects that it can tolerate delay, not that it's less important; it's just as safety-critical as anything else in the system. STATUS is pure telemetry and can wait the longest.

---

## 5. Safety Reasoning (Required Answer #2)

**Why a stale/missing BRAKE matters more than a stale STEER:** losing steering input degrades control, but losing brake input can mean the vehicle can't be stopped — brake is the one command whose absence has to be treated as a worst-case safety event, not just a control-quality issue. That's why BRAKE is the only command type that (a) jumps the queue to the front and is never dropped, and (b) once active, **latches** — `Command_Process` will not let a subsequently-processed THROTTLE or STEER re-enable the physical output while `g_currentBrake > 0`, so a stale queued command arriving after a BRAKE can't silently undo it. Only an explicit `BRAKE 0` releases the lock.

**How the watchdog guarantees an actual fail-safe, not just silence:** the watchdog doesn't just stop updating the display — it actively forces the physical output into a distinct, continuously-driven blink pattern the moment 500 ms passes with no valid command, and logs the transition. This is a positive, visible action rather than "going quiet," which matters because a real CAN node has no way to distinguish "the driver intended this state to persist" from "the sender crashed" — treating prolonged silence as failure, unconditionally, is the only interpretation that's safe by default.

---

## 6. Debugging Journey — What I Found and Fixed

This is the honest account of the bugs found while getting this running on real hardware, in the order they were found:

1. **Task priorities didn't match the intended design** — WATCHDOG was created at the highest priority instead of COMMAND_RX. Fixed by reordering `xTaskCreate` priority arguments.
2. **UART RX was a busy-wait, not an RTOS-aware blocking call** — `UART_ReceiveChar()` spun on a raw register check with no `vTaskDelay`/blocking primitive, so as the highest-priority task, COMMAND_RX could starve every lower-priority task of CPU time entirely while "waiting" for a byte. Fixed by polling `UART_Available()` with a `vTaskDelay(1 tick)` between checks so the scheduler can run other tasks in the gaps.
3. **Brake could be silently overridden by a queued THROTTLE** — `Command_Process`'s throttle case unconditionally cleared `g_currentBrake` and drove the output, so a THROTTLE dequeued right after a BRAKE would undo it. Fixed by removing the unconditional clear and gating the throttle case on `g_currentBrake == 0`, with the throttle command explicitly logged as "ignored" while brake is active.
4. **The watchdog's fail-safe blink and the PWM ISR fought over the same GPIO port** — the watchdog wrote `PORTC` directly while the Timer0 ISR was also continuously rewriting it based on duty cycle, so the blink was invisible, overwritten almost immediately. Fixed by routing the fail-safe blink through `Actuator_SetOutput()` instead, so there's one single source of truth for the output.
5. **Shared multi-byte globals were unprotected** — `g_ulLastHeartbeat` and `g_lastCmdValue` are written by one task and read by another; on an 8-bit AVR a multi-byte write can be interrupted mid-update. Fixed by wrapping the writes in `taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()`.
6. **A 16-bit integer overflow made uptime look permanently stuck at 0** — `xTaskGetTickCount() * 1000 / configTICK_RATE_HZ` was computed in 16-bit arithmetic (since `TickType_t` is `uint16_t` here) before being stored in a `uint32_t`, so it silently wrapped almost immediately after boot. This looked exactly like a deep RTOS tick-timing failure and cost significant debugging time before being traced to this one line. Fixed by casting to `uint32_t` before the multiplication. **Known remaining limitation:** because the underlying tick counter itself is still 16-bit, uptime still rolls over at ~65 seconds — a real hardware-width limit, not a bug, and one I'd address with more time by moving to 32-bit ticks if the port supports it.
7. **A real stack overflow in `vStatusTask`**, caught using FreeRTOS's built-in `configCHECK_FOR_STACK_OVERFLOW = 2` + `vApplicationStackOverflowHook` — `snprintf` with several format arguments needed more stack than the task's original 140-byte allocation. Fixed by increasing `STATUS` and `ACTUATE` task stack sizes (both call `snprintf`) and slightly increasing `configTOTAL_HEAP_SIZE` to fit.
8. **UART bytes could be lost during a fast pasted burst** (task's own "paste two lines at once" test) — the original polled RX only checked for a new byte once per scheduler tick, and the UART hardware only buffers a single byte in `UDR`; a second byte arriving before the first was read would silently overwrite it. Diagnosed by adding raw character markers through `Command_Process` and a raw RX echo, which showed a `BRAKE 100` line vanishing entirely under a pasted burst while working correctly when typed with any gap. Fixed by rewriting UART RX to be interrupt-driven with a small ring buffer (`USART_RXC_vect`), so bytes are captured by hardware the instant they arrive regardless of task scheduling.
9. **Recovery-from-failsafe didn't restore the physical LED when triggered by `PING`** — `PING` and the watchdog's own recovery branch never called `Actuator_SetOutput()`, so if the last physical output state was the fail-safe blink, a `PING` would clear the `LINK LOST` state and log "restored" without ever telling the LED to return to the correct throttle/brake value. Fixed by having the watchdog's recovery branch explicitly re-assert the correct output (0 if brake is active, otherwise current throttle) whenever it clears the fail-safe flag.
10. **STEER could drive the physical output even while brake was active** — the same class of bug as item 3, but in the `'S'` case: it called `Actuator_SetOutput(steerDuty)` unconditionally, so a STEER command could visibly override the braked output even though throttle was already correctly locked out. Fixed by applying the same `g_currentBrake == 0` guard to the STEER case as THROTTLE — `g_currentSteering` still updates and is tracked/logged either way, but the physical output is only driven by STEER when brake is not active. This makes the brake latch apply consistently to every command that can touch the physical output, not just throttle.

---

## 7. What I'd Fix or Add With One More Day (Required Answer #3)

- **Verify the UART fix more rigorously under stress** — the interrupt-driven ring buffer should eliminate byte loss, but I'd want to stress-test with rapid automated bursts (not just manual pastes) to be confident there's no remaining edge case, and add an overflow counter to the ring buffer itself for visibility.
- **Move to 32-bit ticks** (`configUSE_16_BIT_TICKS = 0`) if the port supports it, to remove the ~65 second uptime rollover entirely rather than just documenting it.
- **Add a persistent "fail-safe engaged" flag to the STATUS output** — right now STATUS can print stale last-known throttle/brake values while the physical output is actually overridden by the watchdog's blink; a status task that reflects "failsafe active" explicitly would give a clearer picture of true system state.

---

## 8. Submission Links

- **GitHub repo:** https://github.com/AbdelrahmanHamada05/cuert-rtos-bridge
- **Video:** [Watch the demo video](https://youtube.com/shorts/QWpuJO7d_to?feature=share)
