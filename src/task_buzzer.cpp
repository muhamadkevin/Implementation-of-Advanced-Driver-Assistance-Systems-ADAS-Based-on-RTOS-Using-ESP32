#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "globals.h"
#include <mabutrace.h>

// ============================================================
// TASK: Buzzer Controller (Priority 2)
// WARNING  → beep intermiten (tit-diam-tit-diam)
// DANGER   → bunyi terus (auto rem aktif)
// EMERGENCY→ bunyi terus (interrupt aktif)
// SAFE     → diam
// ============================================================
void vTaskBuzzerController(void *pvParameters) {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    SystemState_t buzzerState = STATE_SAFE;

    for (;;) {
        TRACE_SCOPE("BuzzerLoop");
        // Cek apakah ada update state baru dari BrakeController
        SystemState_t newState;
        if (xQueueReceive(xBuzzerStateQueue, &newState, 0) == pdTRUE) {
            buzzerState = newState;
            // Kalau pindah ke SAFE, langsung matikan buzzer
            if (buzzerState == STATE_SAFE) {
                digitalWrite(PIN_BUZZER, LOW);
            }
        }

        switch (buzzerState) {
            case STATE_WARNING:
                // Beep intermiten: tit (200ms) - diam (200ms)
                if (g_serialLoggingEnabled) Serial.println("[TEST_BUZZER] Beep ON");
                digitalWrite(PIN_BUZZER, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                if (g_serialLoggingEnabled) Serial.println("[TEST_BUZZER] Beep OFF");
                digitalWrite(PIN_BUZZER, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;

            case STATE_DANGER:
            case STATE_EMERGENCY:
                // Bunyi terus-menerus
                if (g_serialLoggingEnabled) Serial.println("[TEST_BUZZER] Solid ON");
                digitalWrite(PIN_BUZZER, HIGH);
                vTaskDelay(pdMS_TO_TICKS(50));  // Yield CPU sedikit
                break;

            case STATE_SAFE:
            default:
                // Diam, tunggu state baru
                if (g_serialLoggingEnabled) Serial.println("[TEST_BUZZER] Solid OFF");
                digitalWrite(PIN_BUZZER, LOW);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}
