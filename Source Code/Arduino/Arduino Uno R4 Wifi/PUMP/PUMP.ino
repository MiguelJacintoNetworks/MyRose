#include <Arduino.h>

#define PUMP_PIN 2

void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  Serial.begin(9600);
  while (!Serial);
  Serial1.begin(9600);
  Serial.println("[UNO R4] PUMP CONTROLLER READY");
}

void loop() {
  if (Serial1.available()) {
    String cmd = Serial1.readStringUntil('\n');
    cmd.trim();
    Serial.print("[UNO R4] COMMAND RECEIVED: ");
    Serial.println(cmd);

    if (cmd == "PUMP_ON") {
      Serial.println("[UNO R4] ACTIVATING PUMP FOR 500 MS");
      digitalWrite(PUMP_PIN, HIGH);
      delay(500);
      digitalWrite(PUMP_PIN, LOW);
      Serial.println("[UNO R4] PUMP CYCLE COMPLETE");
      Serial1.println("SUCCESS");
    } else {
      Serial.print("[UNO R4] UNKNOWN CMD: ");
      Serial.println(cmd);
      Serial1.println("INSUCCESS");
    }
  }
}