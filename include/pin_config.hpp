#pragma once

#include <stdint.h>

namespace transmitter_pins {
constexpr uint8_t LED_GATE = 18;
constexpr uint8_t WARNING_LED = 23;
constexpr uint8_t IR_ACK = 19;

constexpr uint8_t KEYPAD_ROWS[4] = {13, 14, 27, 26};
constexpr uint8_t KEYPAD_COLUMNS[4] = {25, 33, 32, 16};
}  // namespace transmitter_pins

namespace receiver_pins {
constexpr uint8_t LIFI_ADC = 34;
constexpr uint8_t IR_TX = 18;
constexpr uint8_t OLED_SDA = 21;
constexpr uint8_t OLED_SCL = 22;
}  // namespace receiver_pins

