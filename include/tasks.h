#ifndef TASKS_H
#define TASKS_H

// ============================================================
// TASK FUNCTION DECLARATIONS
// ============================================================

// Task 1: Baca sensor ultrasonik, filter, kirim ke queue (Priority 3)
void vTaskUltrasonicReader(void *pvParameters);

// Task 2: Kontrol rem, LED, LCD berdasarkan jarak + emergency (Priority 2)
void vTaskBrakeController(void *pvParameters);

// Task 3: Kontrol buzzer berdasarkan state dari BrakeController (Priority 2)
void vTaskBuzzerController(void *pvParameters);

// Task 4: Log state ke Serial + heartbeat (Priority 1)
void vTaskSerialLogger(void *pvParameters);

// ============================================================
// ISR DECLARATION
// ============================================================

// ISR untuk tombol emergency (GPIO RISING + debounce)
void IRAM_ATTR Emergency_ISR();

#endif // TASKS_H
