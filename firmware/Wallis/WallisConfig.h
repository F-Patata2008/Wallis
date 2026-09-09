#ifndef WALLIS_CONFIG_H
#define WALLIS_CONFIG_H

#include <Arduino.h>

// =========================================================================
//  WALLIS (WALL-I'S) BEACH CLEANER - ESP32 PIN CONFIGURATION & ARCHITECTURE
// =========================================================================

// --- 1. I2C BUS (Shared between LCD 16x2 and RTC Clock module) ---
#define PIN_I2C_SDA          21
#define PIN_I2C_SCL          22
#define LCD_I2C_ADDR         0x27  // Detected address (or 0x3F)
#define RTC_I2C_ADDR         0x68  // Standard DS3231/DS1307 address

// --- 2. SPI BUS: RFID-RC522 (Master Activation / Deactivation Key) ---
// ATTENTION: RC522 VCC must be connected to 3.3V (NOT 5V!)
#define PIN_RFID_SS          5     // SDA/SS pin on RC522
#define PIN_RFID_RST         4     // RST pin on RC522
#define PIN_RFID_SCK         18    // SCK
#define PIN_RFID_MISO        19    // MISO
#define PIN_RFID_MOSI        23    // MOSI

// --- 3. SAFETY & ENVIRONMENTAL SENSORS (Analog inputs: 34, 35 are input-only) ---
#define PIN_WATER_SENSOR     34    // Analog Water Sensor (Tide / Sea water emergency)
#define PIN_PHOTO_RESISTOR   35    // Analog LDR (Night/Day detection with 10k divider)
#define PIN_DHT_DATA         15    // Digital DHT11 (Enclosure temp/humidity)

// Thresholds
#define WATER_ALERT_THRESHOLD 800  // ADC reading above which water is detected
#define NIGHT_THRESHOLD       1200 // ADC reading indicating night/darkness

// --- 4. POWER & ACTUATOR OUTPUTS ---
#define PIN_RELAY_MAIN       2     // Blue 5V Relay: High-power safety switch
#define PIN_BUZZER           14    // Piezo Buzzer for audio chimes & alarms
#define PIN_SERVO_GATE       13    // Micro Servo (SG90) for Sieve / Dump Gate
#define PIN_LED_ACTIVE       27    // Green LED: System Running
#define PIN_LED_ALERT        12    // Red LED: Standby / Water Emergency

// --- 5. FUTURE LOCOMOTION & SIEVE MOTOR CONTROLLERS (PWM & DIR) ---
// Reserved pins for H-Bridge Drivers (L298N / TB6612FNG)
#define PIN_MOTOR_LEFT_PWM   25
#define PIN_MOTOR_RIGHT_PWM  26
#define PIN_MOTOR_SIEVE_PWM  32
#define PIN_MOTOR_SIEVE_DIR  33

// --- SYSTEM STATES ---
enum WallisState {
  STATE_STANDBY = 0,       // Waiting for RFID swipe to start
  STATE_ACTIVE_CLEANING,   // Relay ON, cleaning in progress
  STATE_WATER_EMERGENCY    // Water detected! Immediate shutdown & alarm
};

#endif // WALLIS_CONFIG_H
