#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include "WallisConfig.h"

// --- Global Objects ---
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
Servo gateServo;

// --- State Variables ---
WallisState currentState = STATE_STANDBY;
bool rfidAvailable = false;
unsigned long lastStateUpdate = 0;
unsigned long sessionStartTime = 0;
uint32_t activeSeconds = 0;

void beep(int count, int durationMs = 80) {
  for (int i = 0; i < count; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(durationMs);
    digitalWrite(PIN_BUZZER, LOW);
    if (i < count - 1) delay(50);
  }
}

void setRelay(bool turnOn) {
  // Most blue Arduino 5V relay modules are ACTIVE LOW:
  // LOW = Relay Closed (ON / COM-NO connected)
  // HIGH = Relay Open (OFF / Safe)
  digitalWrite(PIN_RELAY_MAIN, turnOn ? LOW : HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("  WALL-I'S (WALLIS) CONTROL OS   ");
  Serial.println("=================================");

  // 1. Setup GPIO Pins
  pinMode(PIN_RELAY_MAIN, OUTPUT);
  setRelay(false); // Safety first: Relay OFF

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED_ACTIVE, OUTPUT);
  digitalWrite(PIN_LED_ACTIVE, LOW);

  pinMode(PIN_LED_ALERT, OUTPUT);
  digitalWrite(PIN_LED_ALERT, HIGH); // Standby LED on

  pinMode(PIN_WATER_SENSOR, INPUT);
  pinMode(PIN_PHOTO_RESISTOR, INPUT);

  // 2. Initialize I2C and LCD 16x2
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WALL-I'S SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  // 3. Initialize SPI & RFID RC522
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (v == 0x91 || v == 0x92 || v == 0x12) {
    rfidAvailable = true;
    Serial.printf("RFID RC522 detectado (version 0x%02X)\n", v);
  } else {
    Serial.println("RFID no conectado o no detectado (opcional)");
  }

  // 4. Initialize Micro Servo
  ESP32PWM::allocateTimer(0);
  gateServo.setPeriodHertz(50);
  gateServo.attach(PIN_SERVO_GATE, 500, 2400);
  gateServo.write(0); // Posicion compuerta cerrada

  // Startup audio chime
  beep(2, 60);
  delay(1000);
  lcd.clear();
  Serial.println("Sistema listo en modo STANDBY.");
}

void loop() {
  // 1. Safety Check: Water Sensor (Ocean Tide Emergency)
  int waterVal = analogRead(PIN_WATER_SENSOR);
  if (waterVal > WATER_ALERT_THRESHOLD) {
    if (currentState != STATE_WATER_EMERGENCY) {
      currentState = STATE_WATER_EMERGENCY;
      setRelay(false); // Kill power immediately
      digitalWrite(PIN_LED_ACTIVE, LOW);
      digitalWrite(PIN_LED_ALERT, HIGH);
      gateServo.write(90); // Abre compuerta de emergencia
      Serial.printf("¡ALERTA DE AGUA DETECTADA! (Nivel: %d)\n", waterVal);
      beep(5, 120);
    }
  }

  // 2. RFID Keycard Check (Activation / Deactivation Toggle)
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String cardUID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      cardUID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      cardUID += String(rfid.uid.uidByte[i], HEX);
    }
    cardUID.toUpperCase();
    Serial.print("Tarjeta RFID detectada: UID = ");
    Serial.println(cardUID);

    // Toggle states if not in water emergency
    if (currentState == STATE_WATER_EMERGENCY) {
      // Swipe resets water alert if dried
      if (waterVal <= WATER_ALERT_THRESHOLD) {
        currentState = STATE_STANDBY;
        beep(1, 150);
        Serial.println("Alerta de agua reseteada por RFID -> STANDBY");
      }
    } else if (currentState == STATE_STANDBY) {
      currentState = STATE_ACTIVE_CLEANING;
      sessionStartTime = millis();
      setRelay(true); // Power up motors/sieve
      digitalWrite(PIN_LED_ACTIVE, HIGH);
      digitalWrite(PIN_LED_ALERT, LOW);
      gateServo.write(45); // Posicion tamiz activa
      beep(2, 80); // Chime for activation
      Serial.println("Wallis ACTIVADO -> Limpieza iniciada!");
    } else if (currentState == STATE_ACTIVE_CLEANING) {
      currentState = STATE_STANDBY;
      setRelay(false); // Cut power to motors
      digitalWrite(PIN_LED_ACTIVE, LOW);
      digitalWrite(PIN_LED_ALERT, HIGH);
      gateServo.write(0); // Cierra compuerta
      beep(1, 200); // Long beep for deactivation
      Serial.println("Wallis DESACTIVADO -> Modo Standby");
    }

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    delay(500); // Debounce card swipe
  }

  // 3. Environmental & Uptime Check
  int lightVal = analogRead(PIN_PHOTO_RESISTOR);
  bool isNight = (lightVal < NIGHT_THRESHOLD);

  // 4. Update LCD & Serial Monitor every 500ms
  if (millis() - lastStateUpdate > 500) {
    lastStateUpdate = millis();

    switch (currentState) {
      case STATE_STANDBY: {
        lcd.setCursor(0, 0);
        lcd.print("WALLIS: STANDBY ");
        lcd.setCursor(0, 1);
        lcd.print(isNight ? "Noche [RFID OK] " : "Dia   [RFID OK] ");
        break;
      }

      case STATE_ACTIVE_CLEANING: {
        activeSeconds = (millis() - sessionStartTime) / 1000;
        lcd.setCursor(0, 0);
        lcd.print("LIMPIEZA ACTIVA ");
        lcd.setCursor(0, 1);
        char buf[17];
        snprintf(buf, sizeof(buf), "T:%02lu:%02lu Relay:ON", 
                 (unsigned long)(activeSeconds / 60), 
                 (unsigned long)(activeSeconds % 60));
        lcd.print(buf);
        break;
      }

      case STATE_WATER_EMERGENCY: {
        lcd.setCursor(0, 0);
        lcd.print("** ALERTA AGUA **");
        lcd.setCursor(0, 1);
        lcd.print("PARADA EMERGENCIA");
        beep(1, 50);
        break;
      }
    }
  }

  delay(20);
}
