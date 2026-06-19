#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

// ============================================================
// RTOS HANDLES (didefinisikan di globals.cpp)
// ============================================================
extern QueueHandle_t xDistanceQueue;
extern QueueHandle_t xLogQueue;
extern QueueHandle_t xBuzzerStateQueue;
extern SemaphoreHandle_t xEmergencySemaphore;
extern SemaphoreHandle_t xLCDMutex;

// ============================================================
// OBJEK GLOBAL HARDWARE (didefinisikan di globals.cpp)
// ============================================================
extern LiquidCrystal_I2C lcd;
extern Servo remServo;

// ============================================================
// ISR SHARED VARIABLE (didefinisikan di globals.cpp)
// ============================================================
extern volatile TickType_t lastEmergencyTick;

// Flag untuk mengontrol logging serial
extern volatile bool g_serialLoggingEnabled;

#endif // GLOBALS_H
