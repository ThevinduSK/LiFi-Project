#include <Arduino.h>

#include "lifi_protocol.hpp"
#include "pin_config.hpp"

void setup() {
  Serial.begin(lifi_protocol::SERIAL_BAUD);

  pinMode(transmitter_pins::LED_GATE, OUTPUT);
  pinMode(transmitter_pins::WARNING_LED, OUTPUT);
  pinMode(transmitter_pins::IR_ACK, INPUT_PULLUP);

  // Keep both LEDs off until an explicit hardware test is performed.
  digitalWrite(transmitter_pins::LED_GATE, LOW);
  digitalWrite(transmitter_pins::WARNING_LED, LOW);

  delay(500);
  Serial.println("Li-Fi transmitter diagnostic firmware started");
  Serial.println("LED outputs are held LOW; no optical data is being sent yet.");
}

void loop() {
  const int ack_level = digitalRead(transmitter_pins::IR_ACK);
  Serial.printf("Transmitter alive | IR_ACK=%s\n", ack_level == LOW ? "LOW" : "HIGH");
  delay(1000);
}

