#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define PIN_TRIG          12
#define PIN_ECHO          14
#define PIN_EMERGENCY     4
#define PIN_LED           25
#define PIN_SERVO         26
#define PIN_BUZZER        27

// ============================================================
// PARAMETER SISTEM
// ============================================================
#define DIST_DANGER       15
#define DIST_WARNING      30
#define DIST_HYSTERESIS   3
#define DIST_OUT_OF_RANGE 400
#define SENSOR_TIMEOUT_US 30000

// Konstanta untuk Servo 360 Derajat
#define SERVO_SPIN_FORWARD      0    // Putar kencang maju
#define SERVO_SPIN_BACKWARD     180  // Putar kencang mundur
#define SERVO_STOP              90   // Sinyal berhenti
#define SERVO_ACTUATION_TIME_MS 500  // Lama narik kabel rem (ms)

#define EMERGENCY_DEBOUNCE_MS  500
#define LOG_INTERVAL_MS        1000
#define SENSOR_INTERVAL_MS     100

// PENTING: ESP32Servo library pakai LEDC channel mulai dari 0.
// Channel 4 aman karena jauh dari channel yang dipakai servo.
#define LEDC_CH_LED       4    // Channel PWM untuk LED (hindari 0-1, dipakai servo)

// ============================================================
// MOVING AVERAGE FILTER (untuk stabilkan sensor ultrasonik)
// ============================================================
#define MA_FILTER_SIZE    5    // Jumlah sampel untuk rata-rata

#endif // CONFIG_H
