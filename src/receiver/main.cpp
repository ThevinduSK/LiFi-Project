#include <Arduino.h>

#include "lifi_protocol.hpp"
#include "pin_config.hpp"

void setup() {
  Serial.begin(lifi_protocol::SERIAL_BAUD);

  pinMode(receiver_pins::IR_TX, OUTPUT);
  digitalWrite(receiver_pins::IR_TX, LOW);

  pinMode(receiver_pins::LIFI_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(receiver_pins::LIFI_ADC, ADC_11db);

  delay(500);
  Serial.println("Li-Fi receiver diagnostic firmware started");
  Serial.println("IR transmitter is held LOW; printing GPIO34 ADC readings.");
}

void loop() {
  const uint16_t adc_raw = analogRead(receiver_pins::LIFI_ADC);
  Serial.printf("LIFI_ADC=%u\n", adc_raw);
  delay(250);
}

