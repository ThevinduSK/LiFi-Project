#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lifi_protocol {
constexpr uint32_t SERIAL_BAUD = 115200;

constexpr uint8_t MAGIC = 0xD3;
constexpr size_t MAX_MESSAGE_LENGTH = 16;
constexpr size_t HEADER_LENGTH = 3;  // magic, sequence, payload length
constexpr size_t MAX_FRAME_LENGTH = HEADER_LENGTH + MAX_MESSAGE_LENGTH + 1;

// The preamble supplies a bright and dark reference for every frame. Payload
// bits use Manchester coding: 1 = light/dark, 0 = dark/light.
constexpr uint32_t IDLE_BEFORE_FRAME_MS = 60;
constexpr uint32_t SYNC_ON_MS = 60;
constexpr uint32_t CALIBRATION_OFF_MS = 40;
constexpr uint32_t HALF_BIT_US = 10000;
constexpr uint32_t BIT_DURATION_US = HALF_BIT_US * 2;
constexpr uint32_t FRAME_GUARD_MS = 20;

constexpr uint16_t MIN_OPTICAL_CONTRAST = 80;
constexpr uint32_t SYNC_CONFIRM_MS = 5;

constexpr uint32_t ACK_DELAY_MS = 40;
constexpr uint32_t ACK_TIMEOUT_MS = 700;
constexpr uint32_t IR_CARRIER_HZ = 38000;
constexpr uint32_t IR_BURST_US = 600;
constexpr uint32_t IR_GAP_US = 600;
constexpr uint8_t IR_BURST_COUNT = 3;

inline uint8_t crc8(const uint8_t* data, size_t length) {
  uint8_t crc = 0;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80U) != 0U ? static_cast<uint8_t>((crc << 1U) ^ 0x07U)
                                : static_cast<uint8_t>(crc << 1U);
    }
  }
  return crc;
}
}  // namespace lifi_protocol
