#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================
// STATE MACHINE
// ============================================================
typedef enum {
    STATE_SAFE      = 0,
    STATE_WARNING   = 1,
    STATE_DANGER    = 2,
    STATE_EMERGENCY = 3
} SystemState_t;

// ============================================================
// LOG STRUCT
// ============================================================
typedef struct {
    SystemState_t state;
    int           distance;
    uint32_t      timestamp_ms;
} LogEntry_t;

#endif // TYPES_H
