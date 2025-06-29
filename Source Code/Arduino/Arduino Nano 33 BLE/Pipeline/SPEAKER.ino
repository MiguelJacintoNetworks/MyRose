#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Checksum.h"

#define SPEAKER_ID   0xB0
#define SPEAKER_PIN  4
#define TONE_FREQ    1000
#define TONE_DUR     300

extern BLECharacteristic dataCharacteristic;

static bool speakerInitialized = false;

void setupSpeaker() {
  Serial.println("[SPEAKER MODULE] INITIALIZING SPEAKER MODULE...");
  pinMode(SPEAKER_PIN, OUTPUT);
  speakerInitialized = true;
  Serial.println("[SPEAKER MODULE] SPEAKER MODULE INITIALIZED SUCCESSFULLY");
}

bool triggerSpeaker() {
  if (!speakerInitialized || !BLE.connected()) return false;

  Serial.println("[SPEAKER MODULE] TRIGGERING SPEAKER TONE...");
  tone(SPEAKER_PIN, TONE_FREQ, TONE_DUR);
  delay(TONE_DUR + 50);

  uint8_t packet[3];
  packet[0] = SPEAKER_ID;
  packet[1] = 1;
  packet[2] = computeChecksum(packet, 2);

  const uint8_t RETRIES = 3;
  for (uint8_t attempt = 1; attempt <= RETRIES; ++attempt) {
    if (dataCharacteristic.writeValue(packet, sizeof(packet))) {
      Serial.println("[SPEAKER MODULE] SPEAKER PACKET TRANSMITTED");
      return true;
    }
    if (attempt < RETRIES) {
      Serial.println("[SPEAKER MODULE] WARNING: PACKET TRANSMISSION FAILED, RETRYING");
      delay(10);
    }
  }

  Serial.println("[SPEAKER MODULE] ERROR: SPEAKER PACKET TRANSMISSION FAILED AFTER MULTIPLE ATTEMPTS");
  return false;
}