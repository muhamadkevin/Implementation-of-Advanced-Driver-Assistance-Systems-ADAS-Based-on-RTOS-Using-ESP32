# Implementation of Advanced Driver Assistance Systems (ADAS) Based on RTOS Using ESP32

An ESP32-based prototype that simulates a real-time **Advanced Driver Assistance System (ADAS)**, built on top of **FreeRTOS** to explore and demonstrate core Real-Time Operating System principles in a resource-constrained embedded environment.

The system reads distance from an ultrasonic sensor, classifies the situation into safety states (`SAFE`, `WARNING`, `DANGER`, `EMERGENCY`), and responds with automated braking (servo), audible warnings (buzzer), visual indicators (LED + LCD), and emergency override via hardware interrupt — all running as concurrent, priority-scheduled FreeRTOS tasks.

---

## Table of Contents

- [Why FreeRTOS?](#why-freertos)
- [Hardware & Prototype](#hardware--prototype)
- [System Architecture](#system-architecture)
- [Task Breakdown](#task-breakdown)
- [RTOS Mechanisms In-Depth](#rtos-mechanisms-in-depth)
  - [Queues](#1-queues-xqueue)
  - [Binary Semaphore](#2-binary-semaphore)
  - [Mutex](#3-mutex)
- [State Machine](#state-machine)
- [Sensor Filtering](#sensor-filtering)
- [Priority Inversion Demo (PIP)](#priority-inversion-demo-pip)
- [Real-Time Tracing with Perfetto](#real-time-tracing-with-perfetto)
- [Project Structure](#project-structure)
- [How to Build & Flash](#how-to-build--flash)

---

## Why FreeRTOS?

### What is FreeRTOS?

**FreeRTOS** (Free Real-Time Operating System) is an open-source, real-time operating system kernel designed for embedded systems and microcontrollers. Unlike a general-purpose OS (Windows, Linux), FreeRTOS is built for **deterministic behavior** — guaranteeing that high-priority tasks execute within predictable time constraints, which is critical for safety systems like ADAS.

### Why We Chose FreeRTOS for This Project

| Reason | Explanation |
|--------|-------------|
| **Free & Open Source** | FreeRTOS is distributed under the MIT license — completely free for commercial and academic use, with no licensing fees or royalties. |
| **Built into ESP32** | The ESP-IDF framework (and Arduino-ESP32) already includes FreeRTOS as the underlying scheduler. No additional installation or porting is needed — it's ready to use out of the box. |
| **Industry Standard** | FreeRTOS is the most widely used RTOS in the embedded industry (used by AWS IoT, automotive ECUs, medical devices). Learning it provides directly transferable skills. |
| **Rich Primitives** | Provides essential real-time constructs — task scheduling, queues, semaphores, mutexes, and timers — that allow us to study RTOS concepts hands-on. |
| **Lightweight** | Minimal memory footprint (< 10 KB kernel), perfect for the ESP32's constrained environment (520 KB SRAM). |
| **Preemptive Scheduling** | Higher-priority tasks automatically preempt lower-priority ones, ensuring time-critical operations (like braking) are never delayed by non-critical ones (like logging). |

---

## Hardware & Prototype

This project is implemented on **real hardware** as a physical prototype, not just a simulation. The following components are wired to the ESP32:

| Component | GPIO Pin | Role |
|-----------|----------|------|
| **HC-SR04 Ultrasonic Sensor** | TRIG: `GPIO 12`, ECHO: `GPIO 14` | Measures distance to obstacles ahead (simulates forward radar/LiDAR) |
| **360° Continuous Rotation Servo** | `GPIO 26` | Actuates the braking mechanism by pulling/releasing a brake cable |
| **Active Buzzer** | `GPIO 27` | Audible warning — intermittent beep for WARNING, continuous for DANGER/EMERGENCY |
| **LED (PWM-controlled)** | `GPIO 25` | Visual proximity indicator — brightness increases as distance decreases |
| **LCD 16×2 (I2C)** | `SDA/SCL` (default I2C) | Displays current system state and status messages |
| **Emergency Button (Jumper Wire)** | `GPIO 4` | Hardware interrupt trigger — simulates a manual emergency brake button |

### Prototype Concept

```
                    ┌──────────────────────┐
   Obstacle ◄──────│  HC-SR04 Ultrasonic   │──── TRIG/ECHO ────┐
                    └──────────────────────┘                    │
                                                                ▼
                                                    ┌──────────────────┐
    ┌── GPIO 25 (LED)  ◄────────────────────────────│                  │
    ├── GPIO 27 (Buzzer) ◄──────────────────────────│     ESP32        │
    ├── GPIO 26 (Servo)  ◄──────────────────────────│   (FreeRTOS)     │
    ├── I2C (LCD 16×2)   ◄──────────────────────────│                  │
    └── GPIO 4  (Emergency) ──────── ISR ──────────►│                  │
                                                    └──────────────────┘
```

The servo is connected to a brake cable mechanism. When `STATE_DANGER` or `STATE_EMERGENCY` is triggered, the servo rotates forward to **pull the cable** (applying the brake). When the state returns to `SAFE`, the servo rotates backward to **release the cable**.

---

## System Architecture

All four tasks run on **Core 1** of the ESP32 (Core 0 is reserved for Wi-Fi/BT stack by Arduino-ESP32). The FreeRTOS preemptive scheduler manages task execution based on priority levels.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ESP32 Core 1 — FreeRTOS Scheduler               │
│                                                                         │
│   ┌─────────────────────┐    xDistanceQueue    ┌──────────────────────┐ │
│   │  UltrasonicReader   │ ──── (5 × int) ────► │  BrakeController     │ │
│   │   Priority: 3       │                      │   Priority: 2        │ │
│   │   Stack: 3072 B     │                      │   Stack: 4096 B      │ │
│   │   Period: 100 ms    │                      │                      │ │
│   └─────────────────────┘                      │  ┌─ LED (PWM)        │ │
│                                                │  ├─ LCD (I2C+Mutex)  │ │
│   ┌─────────────────────┐   xBuzzerStateQueue  │  ├─ Servo (Brake)    │ │
│   │  BuzzerController   │ ◄── (3 × State) ─── │  └─ State Machine    │ │
│   │   Priority: 2       │                      └──────────────────────┘ │
│   │   Stack: 2048 B     │                        │                      │
│   └─────────────────────┘                        │ xLogQueue            │
│                                                  │ (10 × LogEntry_t)    │
│   ┌─────────────────────┐                        ▼                      │
│   │  SerialLogger       │ ◄──────────────────────┘                      │
│   │   Priority: 1       │                                               │
│   │   Stack: 3072 B     │     ┌──────────────┐                          │
│   └─────────────────────┘     │ Emergency_ISR│──► xEmergencySemaphore   │
│                               │ (GPIO 4 IRQ) │    (Binary Semaphore)    │
│                               └──────────────┘                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Task Breakdown

### Task 1: `vTaskUltrasonicReader` — Sensor Acquisition

| Property | Value |
|----------|-------|
| **Priority** | 3 (Highest among tasks) |
| **Stack** | 3072 bytes |
| **Core** | 1 |
| **Period** | Every 100 ms (`SENSOR_INTERVAL_MS`) |

**What it does:**
1. Sends a 10 µs trigger pulse to the HC-SR04 sensor.
2. Measures the echo return time using `pulseIn()` with a 30 ms timeout (`SENSOR_TIMEOUT_US`).
3. Converts the duration to distance in centimeters: `distance = duration × 0.034 / 2`.
4. Validates the reading — if timeout occurs (`duration == 0`), the reading is **discarded** instead of sending `OUT_OF_RANGE`. This prevents false state transitions.
5. Applies a **Median Filter** (window size = 5) to smooth out noise and outliers.
6. Sends the filtered distance to `xDistanceQueue`.

**Why highest priority?** The sensor reading is the system's primary input. If this task is delayed, all downstream decisions (braking, warnings) are based on stale data. A 100 ms reading interval ensures the system has fresh data at all times.

---

### Task 2: `vTaskBrakeController` — Decision & Actuation Engine

| Property | Value |
|----------|-------|
| **Priority** | 2 |
| **Stack** | 4096 bytes (largest — handles servo, LCD, LED, state logic) |
| **Core** | 1 |

**What it does (per loop iteration):**

1. **Check Emergency Semaphore** — Waits up to 50 ms for `xEmergencySemaphore`. If received → immediately enter `STATE_EMERGENCY`.
2. **Read Distance Queue** — If no emergency, reads latest distance from `xDistanceQueue` and evaluates the state machine (with hysteresis).
3. **Update Indicators First** — Updates LED brightness and LCD display **before** servo actuation. This is critical: servo actuation blocks the task for 500 ms, and if indicators were updated after, the user would experience a noticeable lag in visual/audio feedback.
4. **Notify Buzzer & Logger** — On state change, flushes old buzzer queue and sends the new state. Also creates a `LogEntry_t` and sends it to `xLogQueue`.
5. **Actuate Servo** — If braking state changed: rotates servo forward (brake ON) or backward (brake OFF), each taking 500 ms of blocking time.
6. **Flush Stale Data** — After the 500 ms servo blocking, flushes `xDistanceQueue` to discard stale readings accumulated during the blocking period.
7. **Emergency Recovery** — Holds emergency state for 2 seconds, then releases the brake, resets all indicators, and returns to `STATE_SAFE`.

**Why 4096 bytes stack?** This task has the most complex logic — it manages servo objects, LCD I2C communication, multiple queue operations, and a state machine with hysteresis. The larger stack prevents stack overflow.

---

### Task 3: `vTaskBuzzerController` — Audible Alert System

| Property | Value |
|----------|-------|
| **Priority** | 2 |
| **Stack** | 2048 bytes |
| **Core** | 1 |

**What it does:**
- Receives system state from `xBuzzerStateQueue`.
- Generates different buzzer patterns based on state:
  - `STATE_SAFE` → Buzzer OFF (silent).
  - `STATE_WARNING` → Intermittent beep: 200 ms ON, 200 ms OFF.
  - `STATE_DANGER` / `STATE_EMERGENCY` → Continuous solid tone.

**Why separate from BrakeController?** If the buzzer timing was handled inside `BrakeController`, the 500 ms servo blocking would interrupt the beep pattern. Running it as a separate task ensures the buzzer rhythm is smooth and uninterrupted.

---

### Task 4: `vTaskSerialLogger` — Diagnostics & Monitoring

| Property | Value |
|----------|-------|
| **Priority** | 1 (Lowest) |
| **Stack** | 3072 bytes |
| **Core** | 1 |

**What it does:**
- Reads `LogEntry_t` from `xLogQueue` and prints state transitions to the serial monitor.
- Sends a periodic **heartbeat** every 1 second, reporting free heap memory and remaining stack space — useful for detecting memory leaks during long runs.

**Why lowest priority?** Logging is non-critical. It must never steal CPU time from sensor reading, braking, or buzzer control. By running at priority 1, it only executes when no higher-priority task needs the CPU.

---

### ISR: `Emergency_ISR` — Hardware Interrupt

| Property | Value |
|----------|-------|
| **Type** | GPIO Interrupt (RISING edge) |
| **Pin** | `GPIO 4` (`INPUT_PULLDOWN`) |
| **Debounce** | 500 ms (software, tick-based) |
| **Attribute** | `IRAM_ATTR` (code resides in IRAM for fastest execution) |

**What it does:**
1. On rising edge (jumper wire connected to 3.3V), the ISR fires.
2. Checks if at least 500 ms has passed since the last trigger (debounce).
3. Gives `xEmergencySemaphore` using `xSemaphoreGiveFromISR()`.
4. Calls `portYIELD_FROM_ISR()` if a higher-priority task was woken, forcing an immediate context switch.

**Why the ISR doesn't use a Queue:**
- An ISR must execute as **fast as possible** (microseconds) because it blocks ALL other interrupts.
- `xSemaphoreGiveFromISR()` is faster and lighter than `xQueueSendFromISR()`.
- An emergency event is **binary** (it happened or it didn't) — there is no data payload to transmit. A binary semaphore is the perfect fit: it simply signals "wake up and handle this."
- If a queue were used, we'd need to allocate a data item, copy it, and handle potential queue-full scenarios — all unnecessary overhead inside an ISR.

---

## RTOS Mechanisms In-Depth

### 1. Queues (`xQueue`)

Queues are the **primary inter-task communication mechanism** in this project. They provide a thread-safe FIFO buffer that allows one task to produce data and another task to consume it, without shared variables or race conditions.

#### Queues Used in This Project

| Queue | Depth | Item Size | Producer | Consumer |
|-------|-------|-----------|----------|----------|
| `xDistanceQueue` | **5** items | `sizeof(int)` = 4 bytes | `UltrasonicReader` | `BrakeController` |
| `xLogQueue` | **10** items | `sizeof(LogEntry_t)` = 12 bytes | `BrakeController` | `SerialLogger` |
| `xBuzzerStateQueue` | **3** items | `sizeof(SystemState_t)` = 4 bytes | `BrakeController` | `BuzzerController` |

#### Why These Specific Queue Depths?

**`xDistanceQueue` — Depth 5:**
The ultrasonic sensor produces a new reading every 100 ms. The worst-case scenario is when `BrakeController` is blocked for 500 ms during servo actuation. During those 500 ms, the sensor produces 5 new readings (500 ÷ 100 = 5). A depth of 5 ensures no data is silently lost during normal operation. When the queue is full, the `UltrasonicReader` explicitly drops the **oldest** item and inserts the newest one, ensuring the consumer always gets the most recent data:

```c
// In vTaskUltrasonicReader:
if (uxQueueSpacesAvailable(xDistanceQueue) == 0) {
    int dummy;
    xQueueReceive(xDistanceQueue, &dummy, 0);  // Drop oldest
}
xQueueSend(xDistanceQueue, &filteredDistance, 0);  // Insert newest
```

**`xLogQueue` — Depth 10:**
The logger task runs at the **lowest priority** (1), meaning it only gets CPU time when no other task is running. During busy periods (rapid state transitions, servo actuation), log entries can pile up. A depth of 10 provides a generous buffer to absorb bursts without losing state transition logs.

**`xBuzzerStateQueue` — Depth 3:**
Only **state changes** are sent (not continuous data), so a depth of 3 is sufficient. In the worst case, the state could rapidly transition through multiple states (e.g., SAFE → WARNING → DANGER) before the buzzer task processes any of them. A depth of 3 handles this scenario.

#### Why Queues Are Flushed (Critical Design Decision)

There are **two situations** where queues are explicitly flushed (emptied):

**Situation 1 — After Servo Actuation (Preventing Stale Data):**
When the servo actuates, it blocks the `BrakeController` task for 500 ms. During this time, the `UltrasonicReader` keeps producing fresh readings that pile up in the queue. If the `BrakeController` processed these stale readings after unblocking, it could trigger **another unnecessary servo actuation** (e.g., the obstacle already moved away, but the queue still contains old "close" readings). The fix:

```c
// In vTaskBrakeController — after servo actuation:
if (servoActuated) {
    int dummy;
    while (xQueueReceive(xDistanceQueue, &dummy, 0) == pdTRUE) {}
    // Queue is now empty — next iteration reads fresh data only
}
```

**Situation 2 — Before Sending Buzzer State (Ensuring Latest State):**
When the system state changes, the old state might still be sitting in `xBuzzerStateQueue` waiting to be processed. If we simply add the new state, the buzzer would process the old state first, causing a brief incorrect buzzer pattern. The fix:

```c
// In vTaskBrakeController — on state change:
SystemState_t dummyState;
while (xQueueReceive(xBuzzerStateQueue, &dummyState, 0) == pdTRUE) {}  // Flush old
xQueueSend(xBuzzerStateQueue, &currentState, 0);  // Send latest
```

---

### 2. Binary Semaphore

A binary semaphore is a **signaling mechanism** (not a locking mechanism). It has only two states: "available" (given) or "not available" (taken). In this project, it's used for **ISR-to-task synchronization**.

#### `xEmergencySemaphore` — How It Works


```
    ┌──────────────┐         xSemaphoreGiveFromISR()        ┌──────────────────┐
    │ Emergency_ISR │ ─────────── "SIGNAL!" ──────────────► │ BrakeController   │
    │ (GPIO 4 IRQ)  │                                       │ xSemaphoreTake()  │
    │ < 1 µs exec   │                                       │ blocks up to 50ms │
    └──────────────┘                                        └──────────────────┘
```

1. `BrakeController` calls `xSemaphoreTake(xEmergencySemaphore, pdMS_TO_TICKS(50))` at the top of each loop iteration. If no emergency, it times out after 50 ms and proceeds to read the distance queue.
2. When the emergency button is pressed, `Emergency_ISR` fires and calls `xSemaphoreGiveFromISR()`, changing the semaphore from "not available" to "available."
3. The scheduler immediately wakes `BrakeController` (via `portYIELD_FROM_ISR`), which now successfully takes the semaphore and enters `STATE_EMERGENCY`.
4. After handling the emergency (2-second hold + servo release), the code clears any pending semaphore signal with `xSemaphoreTake(xEmergencySemaphore, 0)` to prevent re-triggering.

#### Why Binary Semaphore (Not Mutex or Queue)?

| Alternative | Why Not |
|-------------|---------|
| **Mutex** | Mutexes implement **Priority Inheritance Protocol** (PIP), which requires task context. Mutexes **cannot be used inside ISRs** because ISRs don't have a task context. |
| **Queue** | Queues work but are heavier — they require memory allocation for data items and copy operations. An emergency is a **binary event** with no payload; a semaphore is the minimal and fastest mechanism. |
| **Task Notification** | Would work but requires knowing the task handle. Binary semaphore is more decoupled. |

#### Why Created with `xSemaphoreCreateBinary()` (Not `xSemaphoreCreateCounting()`)?

A binary semaphore can only hold a **single signal**. Even if the button is pressed multiple times while the task is busy, only one emergency event is registered. This is intentional — multiple emergency triggers don't "stack up," they just mean "there's an emergency" (singular).

---

### 3. Mutex

A mutex (Mutual Exclusion) is a **locking mechanism** used to protect a shared resource from concurrent access. Unlike a binary semaphore, a mutex has **ownership** — only the task that locked it can unlock it — and it supports **Priority Inheritance Protocol (PIP)**.

#### `xLCDMutex` — Protecting the LCD (I2C Bus)

The LCD communicates over the I2C bus, which is **not thread-safe**. If two tasks tried to write to the LCD at the same time, the I2C data stream would be corrupted, causing garbled text or a bus lockup.

```c
// In updateLCD():
if (xSemaphoreTake(xLCDMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
// ── Critical Section: Only one task can reach here at a time ──
lcd.setCursor(0, 0);
lcd.print("STATUS: SAFE    ");
lcd.setCursor(0, 1);
lcd.print("ROAD CLEAR      ");
// ── End Critical Section ──
xSemaphoreGive(xLCDMutex);
```

#### Why 20 ms Timeout (Not `portMAX_DELAY`)?

The `updateLCD()` function uses a **20 ms timeout** instead of waiting forever:
- The LCD update is a **non-critical** operation — if it fails once, the display will be updated on the next state change.
- If the mutex were blocked indefinitely, and the holding task crashed or was delayed, the waiting task would be permanently stuck, potentially causing a system deadlock.
- 20 ms is long enough for a normal I2C transaction to complete, but short enough to not block the `BrakeController` (which calls `updateLCD`).

#### Why Mutex Instead of Binary Semaphore for LCD?

| Feature | Mutex | Binary Semaphore |
|---------|-------|-------------------|
| Ownership | ✅ Only the locker can unlock | ❌ Any task can give/take |
| Priority Inheritance (PIP) | ✅ Yes — prevents priority inversion | ❌ No |
| ISR-safe | ❌ No | ✅ Yes |

The LCD mutex uses **Priority Inheritance Protocol (PIP)**: if a low-priority task holds the mutex and a high-priority task is waiting for it, FreeRTOS temporarily **boosts** the low-priority task's priority to match the waiting task. This prevents **priority inversion** — a scenario where a medium-priority task could preempt the low-priority task and indirectly block the high-priority task.

---

## State Machine

The system operates as a **4-state finite state machine** with **hysteresis** to prevent oscillation at state boundaries:

```
                    dist ≤ 15cm                 dist ≤ 15cm
    ┌──────────┐ ──────────────► ┌──────────┐ ◄────────────── ┌──────────┐
    │   SAFE   │                 │  DANGER  │                  │ EMERGENCY│
    │  LED OFF │ ◄────────────── │ AUTO     │                  │ MANUAL   │
    │  Buzzer  │ dist > 33cm     │ BRAKE    │    2s timeout    │ BRAKE    │
    │   OFF    │                 │ Buzzer   │ ────────────────►│ Buzzer   │
    └────┬─────┘                 │ SOLID ON │                  │ SOLID ON │
         │                       └────┬─────┘                  └──────────┘
         │ dist ≤ 30cm                │ dist > 18cm                 ▲
         ▼                            ▼                             │
    ┌──────────┐                                              GPIO 4 IRQ
    │ WARNING  │                                           (Hardware Interrupt)
    │ Buzzer   │
    │ BEEPING  │
    └──────────┘
```

**Hysteresis values** (`DIST_HYSTERESIS = 3 cm`):
- SAFE → WARNING at ≤ 30 cm, but WARNING → SAFE requires > 33 cm (30 + 3).
- WARNING → DANGER at ≤ 15 cm, but DANGER → WARNING requires > 18 cm (15 + 3).

This 3 cm dead zone prevents rapid state flickering when an object is hovering right at a threshold distance.

---

## Sensor Filtering

The ultrasonic sensor (HC-SR04) is inherently **noisy** — reflections from odd surfaces, temperature variations, and electrical interference can cause sporadic readings. The project implements two layers of filtering:

### Layer 1: Validation (Reject Invalid Readings)
```c
if (duration == 0) {
    // Timeout — sensor didn't receive echo. Skip this reading entirely.
    vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
    continue;
}
if (rawDistance > DIST_OUT_OF_RANGE) rawDistance = DIST_OUT_OF_RANGE;  // Clamp max
if (rawDistance < 2) rawDistance = 2;  // HC-SR04 minimum range is ~2 cm
```

### Layer 2: Median Filter (5-Sample Window)
Instead of averaging (which is affected by outliers), the code uses a **median filter** — it sorts the last 5 readings and takes the middle value. This is robust against spike noise:

```
Readings:  [25, 24, 120, 26, 25]   ← 120 is a spike/noise
Sorted:    [24, 25, 25, 26, 120]
Median:     25  ← Spike is completely ignored!
```

---

## Priority Inversion Demo (PIP)

The project includes a **standalone educational demo** (`demo_pip.cpp`) that can be activated by uncommenting `runPIPDemo()` in `main.cpp`. This demo visually demonstrates the **Priority Inversion problem** and how FreeRTOS's **Priority Inheritance Protocol (PIP)** solves it.

### The Scenario

Three tasks compete:

| Task | Priority | Behavior |
|------|----------|----------|
| **LPT** (Low Priority Task) | 1 | Acquires a shared resource, holds it for 2 seconds |
| **MPT** (Medium Priority Task) | 2 | Starts after 500 ms, consumes CPU for 3 seconds (no resource needed) |
| **HPT** (High Priority Task) | 3 | Starts after 1 second, desperately needs the shared resource |

### Without PIP (Binary Semaphore) — `USE_PIP = 0`

```
Time 0s:   LPT acquires resource
Time 0.5s: MPT wakes up, preempts LPT (higher priority), runs for 3 seconds
Time 1s:   HPT wakes up, needs resource, BLOCKED because LPT still holds it
           BUT LPT can't release it because MPT is hogging the CPU!
Time 3.5s: MPT finishes, LPT finally resumes and releases resource
Time 3.5s: HPT finally gets resource — waited ~2.5 seconds! (PRIORITY INVERSION)
```

**HPT (priority 3) was indirectly blocked by MPT (priority 2), even though MPT doesn't use the resource.**

### With PIP (Mutex) — `USE_PIP = 1`

```
Time 0s:   LPT acquires mutex
Time 0.5s: MPT wakes up
Time 1s:   HPT wakes up, needs mutex → FreeRTOS BOOSTS LPT's priority to 3!
           LPT now runs at priority 3, preempts MPT, finishes work quickly
Time 2s:   LPT releases mutex, priority restored to 1
           HPT immediately gets mutex — waited only ~1 second (CORRECT BEHAVIOR)
```

Toggle `#define USE_PIP 1` or `0` in `demo_pip.cpp` to observe both behaviors via Serial Monitor.

---

## Real-Time Tracing with Perfetto

The project integrates **MabuTrace** to generate trace data compatible with [Perfetto UI](https://ui.perfetto.dev), Google's open-source trace visualization tool. This allows visualizing task execution timing, context switches, and system behavior on a microsecond-level timeline.

### How to Use

1. While the system is running, type **`T`** in Serial Monitor.
2. The system outputs a JSON trace between `===== TRACE DATA START =====` and `===== TRACE DATA END =====`.
3. Copy the JSON, save as a `.json` file.
4. Open [ui.perfetto.dev](https://ui.perfetto.dev) and click **"Open trace file"**.
5. Type **`R`** in Serial Monitor to resume logging.

Each task loop iteration is traced with `TRACE_SCOPE()`, allowing you to see exactly how long each task runs, when context switches happen, and where time is spent.

---

## Project Structure

```
├── include/
│   ├── config.h          # Pin definitions, system parameters, thresholds
│   ├── types.h           # SystemState_t enum, LogEntry_t struct
│   ├── globals.h         # External declarations for RTOS handles & hardware objects
│   ├── tasks.h           # Task function declarations
│   ├── helpers.h         # Helper function declarations (filter, LED, LCD)
│   └── demo_pip.h        # Priority Inversion demo declaration
│
├── src/
│   ├── main.cpp          # setup() and loop() — system initialization
│   ├── globals.cpp       # Global variable definitions (RTOS objects, hardware)
│   ├── task_ultrasonic.cpp  # Ultrasonic sensor reader task
│   ├── task_brake.cpp    # Brake controller task + Emergency ISR
│   ├── task_buzzer.cpp   # Buzzer controller task
│   ├── task_logger.cpp   # Serial logger task
│   ├── helpers.cpp       # Median filter, LED PWM, LCD update functions
│   └── demo_pip.cpp      # Priority Inversion / PIP demo (standalone)
│
├── platformio.ini        # PlatformIO build configuration
└── README.md             # This file
```

---

## How to Build & Flash

### Prerequisites

- [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) (VS Code extension) or PlatformIO CLI
- ESP32 development board
- USB cable

### Dependencies (Auto-installed by PlatformIO)

| Library | Version | Purpose |
|---------|---------|---------|
| `LiquidCrystal_I2C` | ^1.1.4 | LCD 16×2 I2C driver |
| `ESP32Servo` | ^3.0.0 | Servo motor control using LEDC PWM |
| `mabutrace` | ^1.0.0 | Real-time tracing for Perfetto |

### Build & Upload

```bash
# Build
pio run

# Upload to ESP32
pio run --target upload

# Open Serial Monitor (115200 baud)
pio device monitor
```

---

## Key RTOS Concepts Demonstrated

| Concept | Where It's Applied |
|---------|-------------------|
| **Preemptive Scheduling** | 4 tasks with different priorities (1–3) |
| **Inter-Task Communication** | 3 Queues for data passing between tasks |
| **ISR-to-Task Synchronization** | Binary Semaphore from `Emergency_ISR` to `BrakeController` |
| **Resource Protection** | Mutex for LCD I2C bus access |
| **Priority Inheritance Protocol** | `xLCDMutex` + dedicated PIP demo |
| **Task Pinning** | All tasks pinned to Core 1 (`xTaskCreatePinnedToCore`) |
| **Debouncing in ISR** | Tick-based software debounce (500 ms) |
| **Queue Backlog Prevention** | Flushing stale data after blocking operations |
| **Deterministic Timing** | `vTaskDelay()` with `pdMS_TO_TICKS()` for precise intervals |
| **Stack Monitoring** | Heartbeat logs with `uxTaskGetStackHighWaterMark()` |
| **Real-Time Tracing** | MabuTrace + Perfetto UI visualization |
