#include <Arduino.h>
#include <Servo.h>
#include <ArduinoBLE.h>
#include "Checksum.h"

#define SERVO_ID 0xA0
#define SERVO_PIN 7
#define SERVO_DURATION_MS 2000
#define SERVO_TOGGLE_MS 200

extern BLECharacteristic dataCharacteristic;

static Servo servo1;
static bool servoInitialized = false;

void setupServo() {
  Serial.println("[SERVO MODULE] INITIALIZING SERVO MODULE...");
  servo1.attach(SERVO_PIN);
  servo1.write(90);
  delay(500);
  servoInitialized = true;
  Serial.println("[SERVO MODULE] SERVO MODULE INITIALIZED SUCCESSFULLY");
}

bool triggerServo() {
  if (!servoInitialized || !BLE.connected()) return false;

  Serial.println("[SERVO MODULE] TRIGGERING SERVO ACTION...");
  unsigned long start = millis();
  bool toggle = false;
  while (millis() - start < SERVO_DURATION_MS) {
    servo1.write(toggle ? 0 : 180);
    toggle = !toggle;
    delay(SERVO_TOGGLE_MS);
  }
  servo1.write(90);
  delay(100);

  uint8_t packet[3];
  packet[0] = SERVO_ID;
  packet[1] = 1;
  packet[2] = computeChecksum(packet, 2);

  const uint8_t RETRIES = 3;
  for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
    if (dataCharacteristic.writeValue(packet, sizeof(packet))) {
      Serial.println("[SERVO MODULE] SERVO PACKET TRANSMITTED");
      return true;
    }
    if (attempt < RETRIES) {
      Serial.println("[SERVO MODULE] WARNING: PACKET TRANSMISSION FAILED, RETRYING");
      delay(10);
    }
  }

  Serial.println("[SERVO MODULE] ERROR: SERVO PACKET TRANSMISSION FAILED AFTER MULTIPLE ATTEMPTS");
  return false;
}