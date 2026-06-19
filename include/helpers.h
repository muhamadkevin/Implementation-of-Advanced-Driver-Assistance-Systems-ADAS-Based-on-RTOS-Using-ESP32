#ifndef HELPERS_H
#define HELPERS_H

#include "types.h"

// ============================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================

// Moving Average / Median Filter untuk stabilkan sensor ultrasonik
int applyMovingAverage(int newReading);

// Set LED brightness via PWM berdasarkan jarak dan state
void setLEDBrightness(int distance, SystemState_t state);

// Update tampilan LCD (hanya jika state berubah, kecuali forceUpdate=true)
void updateLCD(SystemState_t state, bool forceUpdate = false);

#endif // HELPERS_H
