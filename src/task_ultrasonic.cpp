#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "globals.h"
#include "helpers.h"
#include <mabutrace.h>

// ============================================================
// TASK 1: Ultrasonic Reader (Priority 3)
// Ditambah: validasi pembacaan + moving average filter
// ============================================================
void vTaskUltrasonicReader(void *pvParameters) {
    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);

    // Beri waktu sensor stabil sebelum mulai baca
    vTaskDelay(pdMS_TO_TICKS(500));

    for (;;) {
        TRACE_SCOPE("UltrasonicRead");
        // Pastikan echo LOW sebelum trigger (mencegah false reading)
        // Kadang echo masih HIGH dari pulse sebelumnya
        if (digitalRead(PIN_ECHO) == HIGH) {
            // Tunggu echo selesai dari pulse sebelumnya
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        digitalWrite(PIN_TRIG, LOW);
        delayMicroseconds(5);       // Sedikit lebih lama untuk settle
        digitalWrite(PIN_TRIG, HIGH);
        delayMicroseconds(10);
        digitalWrite(PIN_TRIG, LOW);

        long duration = pulseIn(PIN_ECHO, HIGH, SENSOR_TIMEOUT_US);

        // ---- VALIDASI PEMBACAAN ----
        // Jika timeout (duration=0), JANGAN langsung kirim OUT_OF_RANGE
        // Ini yang bikin state loncat2 dan LED kedip
        if (duration == 0) {
            // Skip pembacaan ini, jangan update queue
            vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
            continue;
        }

        int rawDistance = (int)(duration * 0.034f / 2.0f);
        if (rawDistance > DIST_OUT_OF_RANGE) rawDistance = DIST_OUT_OF_RANGE;
        if (rawDistance < 2) rawDistance = 2;  // HC-SR04 minimum ~2cm

        // Terapkan median filter untuk stabilkan pembacaan
        int filteredDistance = applyMovingAverage(rawDistance);

        if (g_serialLoggingEnabled) {
            Serial.printf("[TEST_ULTRA] Raw: %3d cm | Filtered: %3d cm\n", rawDistance, filteredDistance);
        }

        if (uxQueueSpacesAvailable(xDistanceQueue) == 0) {
            int dummy;
            xQueueReceive(xDistanceQueue, &dummy, 0);
        }
        xQueueSend(xDistanceQueue, &filteredDistance, 0);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
    }
}
