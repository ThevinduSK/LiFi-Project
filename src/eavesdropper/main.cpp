#include <Arduino.h>

#include "lifi_protocol.hpp"

void setup() {
  Serial.begin(lifi_protocol::SERIAL_BAUD);
  delay(500);
  Serial.println("Eavesdropper firmware is intentionally deferred.");
}

void loop() {
  delay(1000);
}

