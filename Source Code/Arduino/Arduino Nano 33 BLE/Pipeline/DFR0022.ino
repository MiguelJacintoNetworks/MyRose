#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Checksum.h"

#define DFR_ID   0xDF
#define DFR_PIN  A0

extern BLECharacteristic dataCharacteristic;

static bool dfrInitialized = false;

void setupDFR0022() {
  Serial.println("[DFR0022] INITIALIZING DFR0022 MODULE...");
  dfrInitialized = true;
  Serial.println("[DFR0022] DFR0022 MODULE INITIALIZED SUCCESSFULLY");
}

bool triggerDFR0022() {
  if (!dfrInitialized || !BLE.connected()) return false;

  int level = analogRead(DFR_PIN);

  uint8_t packet[4];
  packet[0] = DFR_ID;
  packet[1] = level & 0xFF;
  packet[2] = (level >> 8) & 0xFF;
  packet[3] = computeChecksum(packet, 3);

  const uint8_t RETRIES = 3;
  for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
    if (dataCharacteristic.writeValue(packet, sizeof(packet))) {
      Serial.println("[DFR0022] DFR0022 PACKET TRANSMITTED");
      return true;
    }
    if (attempt < RETRIES) {
      Serial.println("[DFR0022] WARNING: PACKET TRANSMISSION FAILED, RETRYING");
      delay(10);
    }
  }

  Serial.println("[DFR0022] ERROR: DFR0022 PACKET TRANSMISSION FAILED AFTER MULTIPLE ATTEMPTS");
  return false;
}