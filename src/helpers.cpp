#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "globals.h"
#include "helpers.h"

// ============================================================
// HELPER: Moving Average Filter
// Menyaring noise pembacaan ultrasonik supaya stabil
// ============================================================
int applyMovingAverage(int newReading) {
    static int buffer[MA_FILTER_SIZE] = {0};
    static int index = 0;
    static bool filled = false;

    buffer[index] = newReading;
    index++;
    if (index >= MA_FILTER_SIZE) {
        index = 0;
        filled = true;
    }

    int count = filled ? MA_FILTER_SIZE : index;
    if (count == 0) return newReading;

    // Sortir buffer lokal untuk median filter (lebih robust dari mean)
    int sorted[MA_FILTER_SIZE];
    memcpy(sorted, buffer, sizeof(int) * count);
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (sorted[j] < sorted[i]) {
                int tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    // Ambil median (nilai tengah) → lebih tahan terhadap outlier
    return sorted[count / 2];
}

// ============================================================
// HELPER: LED brightness via PWM
// Semakin dekat -> semakin terang
// Ditambah smoothing agar tidak kedip
// ============================================================
void setLEDBrightness(int distance, SystemState_t state) {
    static uint8_t currentBrightness = 0;
    uint8_t targetBrightness = 0;

    if (state == STATE_SAFE) {
        targetBrightness = 0;
    } else if (state == STATE_EMERGENCY) {
        targetBrightness = 255;
    } else if (distance >= DIST_WARNING) {
        targetBrightness = 50;
    } else {
        targetBrightness = (uint8_t) map(distance, DIST_DANGER, DIST_WARNING, 255, 50);
        targetBrightness = constrain(targetBrightness, 50, 255);
    }

    // ---- SMOOTHING: transisi bertahap, bukan langsung loncat ----
    // Ini menghilangkan efek kedap-kedip pada LED
    if (state == STATE_EMERGENCY || state == STATE_SAFE) {
        // State kritis: langsung set tanpa smoothing
        currentBrightness = targetBrightness;
    } else {
        // Geser brightness secara bertahap (max 20 step per iterasi)
        int diff = (int)targetBrightness - (int)currentBrightness;
        if (abs(diff) <= 20) {
            currentBrightness = targetBrightness;
        } else if (diff > 0) {
            currentBrightness += 20;
        } else {
            currentBrightness -= 20;
        }
    }

    ledcWrite(LEDC_CH_LED, currentBrightness);
}

// ============================================================
// HELPER: Update LCD (hanya jika state berubah)
// ============================================================
void updateLCD(SystemState_t state, bool forceUpdate) {
    static SystemState_t lastLCDState = (SystemState_t)(-1);
    if (!forceUpdate && state == lastLCDState) return;

    // Ambil mutex terlebih dahulu
    if (xSemaphoreTake(xLCDMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;

    // Update status cache HANYA setelah sukses mengambil mutex dan sebelum menulis ke LCD
    lastLCDState = state;

    // Langsung overwrite tanpa lcd.clear() → menghilangkan flicker
    // Semua string sudah di-pad 16 karakter, jadi sisa karakter lama tertimpa
    lcd.setCursor(0, 0);
    switch (state) {
        case STATE_SAFE:
            lcd.print("STATUS: SAFE    ");
            lcd.setCursor(0, 1);
            lcd.print("ROAD CLEAR      ");
            break;
        case STATE_WARNING:
            lcd.print("STATUS: WARNING ");
            lcd.setCursor(0, 1);
            lcd.print("JAGA JARAK!     ");
            break;
        case STATE_DANGER:
            lcd.print("STATUS: DANGER  ");
            lcd.setCursor(0, 1);
            lcd.print("AUTO BRAKE!     ");
            break;
        case STATE_EMERGENCY:
            lcd.print("!! EMERGENCY !! ");
            lcd.setCursor(0, 1);
            lcd.print("MANUAL BRAKE!   ");
            break;
    }

    xSemaphoreGive(xLCDMutex);
}
