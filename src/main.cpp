// ============================================================
// ADAS - Automatic Braking System (Modular Version)
// ESP32 + FreeRTOS
//
// File ini hanya berisi setup() dan loop().
// Semua task, helper, dan konfigurasi ada di file terpisah.
// ============================================================

#include "config.h"
#include "globals.h"
#include "tasks.h"
#include "types.h"
#include "demo_pip.h"
#include <Arduino.h>
#include <mabutrace.h>

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[ADAS] Booting...");

  // JALANKAN DEMO PRIORITY INVERSION
  // runPIPDemo(); // <-- Di-comment agar ADAS berjalan normal

  // Emergency pin: INPUT_PULLDOWN
  // Saat jumper dihubungkan ke 3V3 → pin naik HIGH → trigger ISR
  pinMode(PIN_EMERGENCY, INPUT_PULLDOWN);

  // LED via PWM (channel 4, 5kHz, 8-bit)
  ledcSetup(LEDC_CH_LED, 5000, 8);
  ledcAttachPin(PIN_LED, LEDC_CH_LED);
  ledcWrite(LEDC_CH_LED, 0);

  // RTOS Objects (buat dulu sebelum attach interrupt!)
  xDistanceQueue = xQueueCreate(5, sizeof(int));
  xLogQueue = xQueueCreate(10, sizeof(LogEntry_t));
  xBuzzerStateQueue = xQueueCreate(3 , sizeof(SystemState_t));
  xEmergencySemaphore = xSemaphoreCreateBinary();
  xLCDMutex = xSemaphoreCreateMutex();

  if (!xDistanceQueue || !xLogQueue || !xBuzzerStateQueue ||
      !xEmergencySemaphore || !xLCDMutex) {
    Serial.println("[ADAS] FATAL: RTOS object creation failed!");
    while (true) {
      delay(1000);
    }
  }

  // Interrupt - attach SETELAH semaphore sudah dibuat
  // Ini penting! Kalau interrupt trigger sebelum semaphore ada → crash
  attachInterrupt(digitalPinToInterrupt(PIN_EMERGENCY), Emergency_ISR, RISING);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  ADAS BOOTING  ");
  lcd.setCursor(0, 1);
  lcd.print("  Please wait.. ");

  // Delay sedikit untuk LCD tampil
  delay(1000);

  // Buat semua task di Core 1
  xTaskCreatePinnedToCore(vTaskUltrasonicReader, "UltrasonicTask", 3072, NULL,
                          3, NULL, 1);
  xTaskCreatePinnedToCore(vTaskBrakeController, "BrakeTask", 4096, NULL, 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(vTaskBuzzerController, "BuzzerTask", 2048, NULL, 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(vTaskSerialLogger, "SerialTask", 3072, NULL, 1, NULL,
                          1);

  // Inisialisasi MabuTrace
  mabutrace_init();

  Serial.println("[ADAS] All tasks created. System running.");
  Serial.println("[ADAS] Ketik 'T' di Serial Monitor untuk dump trace data.");
  Serial.println("[ADAS] Buka ui.perfetto.dev lalu paste hasilnya.");
}

// Callback untuk menulis trace JSON ke Serial secara chunked
static void serial_trace_writer(void *ctx, const char *data, size_t len) {
  Serial.write((const uint8_t *)data, len);
}

void loop() {
  // Cek apakah user mengetik command di Serial Monitor
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'T' || c == 't') {
      g_serialLoggingEnabled = false; // Matikan logging serial sementara dump
      Serial.println("\n===== TRACE DATA START =====");
      get_json_trace_chunked(NULL, serial_trace_writer);
      Serial.println("\n===== TRACE DATA END =====");
      Serial.println("Copy JSON di atas, simpan sebagai .json");
      Serial.println("Buka ui.perfetto.dev, klik 'Open trace file'.");
      Serial.println("Ketik 'R' untuk melanjutkan Serial Logging.");
    } else if (c == 'R' || c == 'r') {
      g_serialLoggingEnabled = true;
      Serial.println("\n[ADAS] Serial Logging Resumed.");
    }
  }
  vTaskDelay(pdMS_TO_TICKS(100));
}
