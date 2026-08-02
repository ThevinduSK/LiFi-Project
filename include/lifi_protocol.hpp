#pragma once

#include <stdint.h>

namespace lifi_protocol {
constexpr uint32_t SERIAL_BAUD = 115200;

// Conservative bring-up timing. These values will be tuned only after the
// receiver ADC test shows a clean difference between LED-off and LED-on levels.
constexpr uint32_t SYNC_ON_MS = 80;
constexpr uint32_t CALIBRATION_OFF_MS = 40;
constexpr uint32_t BIT_DURATION_MS = 20;
}  // namespace lifi_protocol

