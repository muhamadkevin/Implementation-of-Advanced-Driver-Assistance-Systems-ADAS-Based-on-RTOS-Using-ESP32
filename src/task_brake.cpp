#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "globals.h"
#include "helpers.h"
#include <mabutrace.h>

// ============================================================
// ISR - EMERGENCY (dengan debounce yang lebih aman)
// ============================================================
void IRAM_ATTR Emergency_ISR() {
    TickType_t now = xTaskGetTickCountFromISR();
    if ((now - lastEmergencyTick) > pdMS_TO_TICKS(EMERGENCY_DEBOUNCE_MS)) {
        lastEmergencyTick = now;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(xEmergencySemaphore, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// ============================================================
// TASK 2: Brake Controller (Priority 2)
// ============================================================
// FIX #1: Servo 360 butuh sinyal PWM 90° TERUS-MENERUS untuk berhenti.
// Jangan pernah detach! Cukup write(SERVO_STOP) dan biarkan attached.

void vTaskBrakeController(void *pvParameters) {
    remServo.attach(PIN_SERVO);   // Attach sekali, JANGAN PERNAH detach!
    remServo.write(SERVO_STOP);    // Pastikan diam saat boot

    SystemState_t currentState = STATE_SAFE;
    SystemState_t prevState    = STATE_SAFE;
    int distance = DIST_OUT_OF_RANGE;
    bool lastBrakeState = false;   // Track posisi servo terakhir
    TickType_t lastServoWriteTick = 0;

    for (;;) {
        TRACE_SCOPE("BrakeControlLoop");
        // ---- 1. INPUT: SINKRONISASI ASINKRON & SENSOR ----
        if (xSemaphoreTake(xEmergencySemaphore, pdMS_TO_TICKS(50)) == pdTRUE) {
            currentState = STATE_EMERGENCY;
        } 
        else if (xQueueReceive(xDistanceQueue, &distance, 0) == pdTRUE) {
            switch (currentState) {
                case STATE_SAFE:
                    if (distance <= DIST_DANGER)
                        currentState = STATE_DANGER;
                    else if (distance <= DIST_WARNING)
                        currentState = STATE_WARNING;
                    break;
                case STATE_WARNING:
                    if (distance <= DIST_DANGER)
                        currentState = STATE_DANGER;
                    else if (distance > DIST_WARNING + DIST_HYSTERESIS)
                        currentState = STATE_SAFE;
                    break;
                case STATE_DANGER:
                    if (distance > DIST_WARNING + DIST_HYSTERESIS)
                        currentState = STATE_SAFE;
                    else if (distance > DIST_DANGER + DIST_HYSTERESIS)
                        currentState = STATE_WARNING;
                    break;
                case STATE_EMERGENCY:
                    break;
            }
        }

        // ---- 2. UPDATE INDIKATOR DULUAN (LED, LCD, Buzzer, Log) ----
        // Lakukan ini sebelum servo memblokir task (vTaskDelay) agar tidak ada lag visual/audio
        setLEDBrightness(distance, currentState);
        updateLCD(currentState);

        if (currentState != prevState) {
            // Flush queue lama supaya buzzer dapat state terbaru seketika
            SystemState_t dummyState;
            while (xQueueReceive(xBuzzerStateQueue, &dummyState, 0) == pdTRUE) {}
            xQueueSend(xBuzzerStateQueue, &currentState, 0);

            const char* stNames[] = {"SAFE","WARNING","DANGER","EMERGENCY"};
            TRACE_INSTANT(stNames[currentState]);
            LogEntry_t logEntry = {
                .state        = currentState,
                .distance     = distance,
                .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount())
            };
            xQueueSend(xLogQueue, &logEntry, 0);
            prevState = currentState;
        }

        // ---- 3. AKTUASI SERVO 360 (Memblokir task 500ms) ----
        bool brakeOn = (currentState == STATE_DANGER || currentState == STATE_EMERGENCY);
        if (brakeOn != lastBrakeState) {
            TickType_t now = xTaskGetTickCount();
            bool servoActuated = false;

            if (brakeOn) {
                // MAU NGEREM: Tarik kabel rem!
                remServo.write(SERVO_SPIN_FORWARD);
                vTaskDelay(pdMS_TO_TICKS(SERVO_ACTUATION_TIME_MS)); // Blokir 500ms
                remServo.write(SERVO_STOP); // Berhenti, PWM tetap aktif
                
                lastBrakeState = true;
                lastServoWriteTick = xTaskGetTickCount();
                servoActuated = true;
            } else {
                // MAU LEPAS REM: Tahan dulu minimal 1 detik sejak pengereman!
                if ((now - lastServoWriteTick) >= pdMS_TO_TICKS(1000)) {
                    remServo.write(SERVO_SPIN_BACKWARD);
                    vTaskDelay(pdMS_TO_TICKS(SERVO_ACTUATION_TIME_MS)); // Blokir 500ms
                    remServo.write(SERVO_STOP); // Berhenti, PWM tetap aktif
                    
                    lastBrakeState = false;
                    lastServoWriteTick = xTaskGetTickCount();
                    servoActuated = true;
                }
            }

            // CEGAH BACKLOG: Karena kita habis memblokir task selama 500ms, queue jarak
            // dari ultrasonik pasti penuh dengan data basi. Hapus semuanya agar kita 
            // membaca data terbaru di iterasi loop berikutnya! Ini mencegah servo gerak 2x.
            if (servoActuated) {
                int dummy;
                while (xQueueReceive(xDistanceQueue, &dummy, 0) == pdTRUE) {}
            }
        }

        // ---- 4. EMERGENCY: Tahan 2 detik lalu reset ----
        if (currentState == STATE_EMERGENCY) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            currentState = STATE_SAFE;
            prevState    = STATE_EMERGENCY;
            
            // Lepas rem (muter mundur)
            remServo.write(SERVO_SPIN_BACKWARD);
            vTaskDelay(pdMS_TO_TICKS(SERVO_ACTUATION_TIME_MS));
            remServo.write(SERVO_STOP);
            
            lastBrakeState = false;
            lastServoWriteTick = xTaskGetTickCount();
            
            ledcWrite(LEDC_CH_LED, 0);
            SystemState_t safeState = STATE_SAFE;
            SystemState_t dummyBuzzer;
            while (xQueueReceive(xBuzzerStateQueue, &dummyBuzzer, 0) == pdTRUE) {}
            xQueueSend(xBuzzerStateQueue, &safeState, 0);
            updateLCD(STATE_SAFE, true);

            xSemaphoreTake(xEmergencySemaphore, 0);
            
            // Hapus backlog queue lagi setelah emergency selesai memblokir
            int dummy;
            while (xQueueReceive(xDistanceQueue, &dummy, 0) == pdTRUE) {}
        }
    }
}
