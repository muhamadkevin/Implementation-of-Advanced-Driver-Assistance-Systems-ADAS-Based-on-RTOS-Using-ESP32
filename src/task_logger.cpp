#include "config.h"
#include "globals.h"
#include "types.h"
#include <Arduino.h>
#include <mabutrace.h>

// ============================================================
// TASK 3: Serial Logger (Priority 1)
// ============================================================
void vTaskSerialLogger(void *pvParameters) {
  LogEntry_t entry;
  uint32_t lastHeartbeatMs = 0;
  const char *stateNames[] = {"SAFE", "WARNING", "DANGER", "EMERGENCY"};

  for (;;) {
    TRACE_SCOPE("LogCycle");
    if (xQueueReceive(xLogQueue, &entry, pdMS_TO_TICKS(LOG_INTERVAL_MS)) ==
        pdTRUE) {
      if (g_serialLoggingEnabled) {
        Serial.printf("[%8lu ms] [TEST_LOGGER] STATE -> %-10s | Dist: %3d cm\n",
                      entry.timestamp_ms, stateNames[entry.state],
                      entry.distance);
      }
    }

    uint32_t now = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
    if (now - lastHeartbeatMs >= LOG_INTERVAL_MS) {
      lastHeartbeatMs = now;
      if (g_serialLoggingEnabled) {
        Serial.printf("[%8lu ms] [TEST_LOGGER] HEARTBEAT | Heap: %lu B | Stack: %u words\n",
                      now, (unsigned long)esp_get_free_heap_size(),
                      (unsigned)uxTaskGetStackHighWaterMark(NULL));
      }
    }
  }
}
