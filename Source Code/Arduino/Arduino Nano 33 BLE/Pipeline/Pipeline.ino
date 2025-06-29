#include <Arduino.h>
#include <ArduinoBLE.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Checksum.h"

#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define SERVICE_UUID        "19B10010-E8F2-537E-4F6C-D104768A1214"
#define DATA_UUID           "19B10012-E8F2-537E-4F6C-D104768A1214"
#define CMD_UUID            "19B10013-E8F2-537E-4F6C-D104768A1214"
#define HANDSHAKE_UUID      "19B10011-E8F2-537E-4F6C-D104768A1214"

#define DFR0022_ID          0xDF
#define TEMP_ID             0xE2
#define SOIL_ID             0xD3
#define SERVO_ID            0xA0
#define PIR_ID              0xC1
#define SPEAKER_ID          0xB0
#define PUMP_ID             0xD4

const unsigned long HEARTBEAT_INTERVAL = 25000UL;
const unsigned long DFR_INTERVAL       =  8000UL;
const unsigned long PIR_INTERVAL       = 15000UL;
const unsigned long TEMP_INTERVAL      =  5000UL;
const unsigned long SOIL_INTERVAL      = 10000UL;

BLEService customService(SERVICE_UUID);
BLECharacteristic dataCharacteristic(DATA_UUID, BLENotify, 50);
BLECharacteristic cmdCharacteristic(CMD_UUID, BLEWrite, 50);
BLECharacteristic handshakeCharacteristic(HANDSHAKE_UUID, BLERead | BLEWrite, 20);

extern void setupServo();
extern bool triggerServo();
extern void setupPIR();
extern bool triggerPIR();
extern void setupSpeaker();
extern bool triggerSpeaker();
extern void setupDFR0022();
extern bool triggerDFR0022();
extern void setupTemperature();
extern bool triggerTemperature();
extern void setupSoilMoisture();
extern bool triggerSoilMoisture();
extern void setupVoice();
extern void loopVoice();
extern int  getLastVoiceResult();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

unsigned long lastHeartbeatTime;
unsigned long lastDFRTime;
unsigned long lastServoTime;
unsigned long lastPIRTime;
unsigned long lastSpeakerTime;
unsigned long lastTempTime;
unsigned long lastSoilTime;
unsigned long handshakeTimestamp;

bool wasCentralConnected = false;
bool centralReady        = false;
int  lastVoiceResult     = -1;

void handshakeWritten(BLEDevice central, BLECharacteristic ch) {
  int len = ch.valueLength();
  if (len < 1 || len > 20) {
    Serial.print("[PIPELINE] [ERROR] HANDSHAKE LENGTH INVALID: ");
    Serial.println(len);
    centralReady = false;
    return;
  }
  uint8_t buf[21];
  ch.readValue(buf, len);
  String v = String((char*)buf, len);
  v.trim();
  Serial.print("[PIPELINE] [INFO] HANDSHAKE RECEIVED: '");
  Serial.print(v);
  Serial.println("'");
  if (v == "READY") {
    centralReady = true;
    handshakeTimestamp = millis();
    handshakeCharacteristic.setValue("READY");
    Serial.println("[PIPELINE] [INFO] HANDSHAKE: CENTRAL CONFIRMED READY");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("[PIPELINE] HANDSHAKE OK");
    display.display();
  } else {
    Serial.println("[PIPELINE] [ERROR] HANDSHAKE NEGOTIATION FAILED");
  }
}

void fileWritten(BLEDevice central, BLECharacteristic characteristic) {
  int len = characteristic.valueLength();
  if (len < 4 || len % 4 != 0) return;

  uint8_t buf[64];
  characteristic.readValue(buf, len);

  Serial.print("[PIPELINE] [INFO] RECEIVED DOWNSTREAM PACKET: ");
  for (int i = 0; i < len; ++i) {
    if (buf[i] < 0x10) Serial.print('0');
    Serial.print(buf[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  display.clearDisplay();
  display.setCursor(0, 0);

  int y = 10;
  for (int i = 0; i < len; i += 4) {
    uint8_t sid = buf[i];
    uint8_t lsb = buf[i+1];
    uint8_t msb = buf[i+2];
    uint8_t chk = buf[i+3];
    uint16_t value = uint16_t(lsb) | (uint16_t(msb) << 8);
    uint8_t sum = (sid + lsb + msb) & 0xFF;

    if (sum != chk) {
      Serial.print("[PIPELINE] [WARNING] CHECKSUM MISMATCH FOR ID 0x");
      Serial.println(sid, HEX);
      continue;
    }

    const char* name = "UNKNOWN";
    switch (sid) {
      case DFR0022_ID: name = "LIGHT INTENSITY"; break;
      case TEMP_ID:    name = "TEMPERATURE";     break;
      case SOIL_ID:    name = "SOIL MOISTURE";   break;
      case SERVO_ID:   name = "SERVO ACTUATOR";  break;
      case PIR_ID:     name = "MOTION SENSOR";   break;
      case SPEAKER_ID: name = "SPEAKER ACTUATOR";break;
    }

    Serial.print("[PIPELINE] [INFO] SENSOR ");
    Serial.print(name);
    Serial.print(" (0x"); Serial.print(sid, HEX); Serial.print(") VALUE = ");
    Serial.println(value);

    display.setCursor(0, y);
    display.print(name);
    display.print(": ");
    display.println(value);
    y += 10;
    if (y > SCREEN_HEIGHT - 10) break;
  }

  display.display();
  Serial.println("[PIPELINE] [INFO] DISPLAY UPDATED");
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  while (!Serial);
  Serial.println("[PIPELINE] [INFO] INITIALIZING PIPELINE");
  Serial.println();

  if (!BLE.begin()) {
    Serial.println("[PIPELINE] [ERROR] FAILED TO INITIALIZE BLE");
    while (1);
  }
  BLE.setLocalName("ARDUINOBLE");
  BLE.setAdvertisedService(customService);
  customService.addCharacteristic(dataCharacteristic);
  customService.addCharacteristic(cmdCharacteristic);
  customService.addCharacteristic(handshakeCharacteristic);
  handshakeCharacteristic.setEventHandler(BLEWritten, handshakeWritten);
  cmdCharacteristic.setEventHandler(BLEWritten, fileWritten);
  BLE.addService(customService);
  BLE.advertise();
  Serial.println("[PIPELINE] [INFO] BLE ADVERTISING STARTED");
  Serial.println();

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[PIPELINE] [ERROR] OLED INITIALIZATION FAILED");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED READY");
  display.display();
  Serial.println("[PIPELINE] [INFO] OLED INITIALIZED AND READY");
  Serial.println();
  Serial.println("[PIPELINE] [INFO] INITIALIZING SENSORS AND ACTUATORS");
  setupDFR0022();       
  Serial.println("[PIPELINE] [INFO] LIGHT SENSOR INITIALIZED"); 
  Serial.println();
  setupTemperature();   
  Serial.println("[PIPELINE] [INFO] TEMPERATURE SENSOR INITIALIZED"); 
  Serial.println();
  setupSoilMoisture();  
  Serial.println("[PIPELINE] [INFO] SOIL MOISTURE SENSOR INITIALIZED"); 
  Serial.println();
  setupPIR();           
  Serial.println("[PIPELINE] [INFO] MOTION SENSOR INITIALIZED"); 
  Serial.println();
  setupServo();         
  Serial.println("[PIPELINE] [INFO] SERVO ACTUATOR INITIALIZED"); 
  Serial.println();
  setupSpeaker();       
  Serial.println("[PIPELINE] [INFO] SPEAKER ACTUATOR INITIALIZED"); 
  Serial.println();
  setupVoice();
  Serial.println("[PIPELINE] [INFO] VOICE MODULE INITIALIZED");
  Serial.println();
  Serial.println("[PIPELINE] [INFO] ALL MODULES INITIALIZED"); 
  Serial.println();

  lastHeartbeatTime = millis();
  lastDFRTime       = millis();
  lastTempTime      = millis();
  lastSoilTime      = millis();
  lastPIRTime       = millis();
  lastServoTime     = millis();
  lastSpeakerTime   = millis();
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "VOICE") {
      Serial.println("[PIPELINE] [INFO] TRIGGERING VOICE INFERENCE");
      loopVoice();
      const char* label = getLastVoiceLabel();
      if (strcmp(label, "Servo") == 0) {
        Serial.println("[PIPELINE] [INFO] VOICE PREDICTION SERVO - ACTIVATING SERVO");
        triggerServo();
      }
      else if (strcmp(label, "Hello") == 0) {
        Serial.println("[PIPELINE] [INFO] VOICE PREDICTION HELLO - TRIGGERING SPEAKER");
        triggerSpeaker();
      }
      else if (strcmp(label, "Water") == 0) {
        Serial.println("[PIPELINE] [INFO] VOICE PREDICTION WATER - TRIGGERING PUMP");
        Serial1.println("PUMP_ON");
        unsigned long start = millis();
        String resp = "";
        while (millis() - start < 1000) {
          if (Serial1.available()) {
            resp = Serial1.readStringUntil('\n');
            resp.trim();
            break;
          }
        }
        bool success = (resp == "SUCCESS");
        uint8_t pkt[3];
        pkt[0] = PUMP_ID;
        pkt[1] = success ? 1 : 0;
        pkt[2] = computeChecksum(pkt, 2);
        dataCharacteristic.writeValue(pkt, 3);
        Serial.print("[PIPELINE] [INFO] PUMP PACKET SENT: ");
        Serial.println(success ? "SUCCESS" : "INSUCCESS");
      }
      else {
        Serial.println("[PIPELINE] [INFO] VOICE PREDICTION UNKNOWN - END CYCLE");
      }
    }
  }
  BLE.poll();

  BLEDevice central = BLE.central();
  bool connected = central && central.connected();
  if (connected != wasCentralConnected) {
    wasCentralConnected = connected;
    Serial.print("[PIPELINE] [INFO] BLE CENTRAL: ");
    Serial.println(connected ? "CONNECTED" : "DISCONNECTED");
    if (!connected) centralReady = false;
  }

  if (millis() - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    lastHeartbeatTime = millis();
    Serial.println("[PIPELINE] [INFO] HEARTBEAT: SYSTEM ACTIVE");
  }

  if (!centralReady) {
    Serial.println("[PIPELINE] [INFO] AWAITING HANDSHAKE");
    delay(500);
    return;
  }

  if (millis() - handshakeTimestamp < 5000) {
    Serial.println("[PIPELINE] [INFO] POST-HANDSHAKE GRACE PERIOD");
    delay(500);
    return;
  }

  if (millis() - lastDFRTime >= DFR_INTERVAL) {
    Serial.println("[PIPELINE] [INFO] TRIGGERING LIGHT SENSOR");
    if (triggerDFR0022()) Serial.println("[PIPELINE] [INFO] LIGHT SENSOR DATA SENT");
                       else Serial.println("[PIPELINE] [ERROR] LIGHT SENSOR TRANSMISSION FAILED");
    lastDFRTime = millis();
  }

  if (millis() - lastTempTime >= TEMP_INTERVAL) {
    Serial.println("[PIPELINE] [INFO] TRIGGERING TEMPERATURE SENSOR");
    if (triggerTemperature()) Serial.println("[PIPELINE] [INFO] TEMPERATURE SENSOR DATA SENT");
                             else Serial.println("[PIPELINE] [ERROR] TEMPERATURE SENSOR TRANSMISSION FAILED");
    lastTempTime = millis();
  }

  if (millis() - lastSoilTime >= SOIL_INTERVAL) {
    Serial.println("[PIPELINE] [INFO] TRIGGERING SOIL MOISTURE SENSOR");
    if (triggerSoilMoisture()) Serial.println("[PIPELINE] [INFO] SOIL MOISTURE SENSOR DATA SENT");
                              else Serial.println("[PIPELINE] [ERROR] SOIL MOISTURE TRANSMISSION FAILED");
    lastSoilTime = millis();
  }

  if (millis() - lastPIRTime >= PIR_INTERVAL) {
    Serial.println("[PIPELINE] [INFO] TRIGGERING MOTION SENSOR");
    if (triggerPIR()) Serial.println("[PIPELINE] [INFO] MOTION SENSOR DATA SENT");
                  else Serial.println("[PIPELINE] [ERROR] MOTION SENSOR TRANSMISSION FAILED");
    lastPIRTime = millis();
  }
}