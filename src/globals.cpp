#include "globals.h"

// ============================================================
// DEFINISI VARIABEL GLOBAL
// Semua variabel yang di-extern di globals.h didefinisikan di sini
// ============================================================

// RTOS Handles
QueueHandle_t xDistanceQueue = NULL;
QueueHandle_t xLogQueue = NULL;
QueueHandle_t xBuzzerStateQueue = NULL;
SemaphoreHandle_t xEmergencySemaphore = NULL;
SemaphoreHandle_t xLCDMutex = NULL;

// Objek Hardware
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo remServo;

// ISR Shared Variable
volatile TickType_t lastEmergencyTick = 0;

// Flag untuk mengontrol logging serial
volatile bool g_serialLoggingEnabled = true;
