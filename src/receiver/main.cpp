#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "lifi_protocol.hpp"
#include "pin_config.hpp"

namespace {
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int8_t OLED_RESET_PIN = -1;
constexpr uint8_t IR_PWM_CHANNEL = 0;
constexpr uint8_t IR_PWM_RESOLUTION_BITS = 8;
constexpr uint8_t IR_PWM_DUTY = 128;

Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET_PIN);
bool display_available = false;
int32_t idle_level = 0;
uint32_t last_idle_report_ms = 0;
bool has_last_sequence = false;
uint8_t last_sequence = 0;

struct ReceivedPacket {
  uint8_t sequence;
  uint8_t length;
  char payload[lifi_protocol::MAX_MESSAGE_LENGTH + 1];
};

uint16_t readAdcAverage(uint8_t sample_count = 6) {
  uint32_t total = 0;
  for (uint8_t sample = 0; sample < sample_count; ++sample) {
    total += analogRead(receiver_pins::LIFI_ADC);
    delayMicroseconds(100);
  }
  return static_cast<uint16_t>(total / sample_count);
}

void waitUntilMicros(uint32_t target_time) {
  while (static_cast<int32_t>(micros() - target_time) < 0) {
    delayMicroseconds(50);
  }
}

bool isLightLevel(uint16_t sample, uint16_t bright_level, uint16_t dark_level) {
  return abs(static_cast<int32_t>(sample) - bright_level) <
         abs(static_cast<int32_t>(sample) - dark_level);
}

bool detectPreamble(uint32_t& frame_started_at, uint16_t& bright_level,
                    uint16_t& dark_level) {
  const uint16_t sample = readAdcAverage(3);
  const int32_t difference = abs(static_cast<int32_t>(sample) - idle_level);

  if (difference < lifi_protocol::MIN_OPTICAL_CONTRAST) {
    idle_level = ((idle_level * 31) + sample) / 32;
    delay(1);
    return false;
  }

  const uint32_t candidate_start = micros();
  delay(lifi_protocol::SYNC_CONFIRM_MS);
  const uint16_t confirmation = readAdcAverage(4);
  if (abs(static_cast<int32_t>(confirmation) - idle_level) <
      lifi_protocol::MIN_OPTICAL_CONTRAST) {
    return false;
  }

  frame_started_at = candidate_start;

  waitUntilMicros(frame_started_at + (lifi_protocol::SYNC_ON_MS / 2U) * 1000U);
  bright_level = readAdcAverage(10);

  waitUntilMicros(frame_started_at +
                  (lifi_protocol::SYNC_ON_MS +
                   lifi_protocol::CALIBRATION_OFF_MS / 2U) *
                      1000U);
  dark_level = readAdcAverage(10);

  const uint16_t contrast = static_cast<uint16_t>(
      abs(static_cast<int32_t>(bright_level) - dark_level));
  Serial.printf("Preamble: bright=%u dark=%u contrast=%u\n", bright_level,
                dark_level, contrast);

  return contrast >= lifi_protocol::MIN_OPTICAL_CONTRAST;
}

bool readManchesterBit(uint32_t data_started_at, size_t bit_index,
                       uint16_t bright_level, uint16_t dark_level,
                       bool& value) {
  const uint32_t bit_started_at =
      data_started_at + bit_index * lifi_protocol::BIT_DURATION_US;

  waitUntilMicros(bit_started_at + lifi_protocol::HALF_BIT_US / 2U);
  const uint16_t first_half = readAdcAverage(5);

  waitUntilMicros(bit_started_at + lifi_protocol::HALF_BIT_US +
                  lifi_protocol::HALF_BIT_US / 2U);
  const uint16_t second_half = readAdcAverage(5);

  const bool first_is_light =
      isLightLevel(first_half, bright_level, dark_level);
  const bool second_is_light =
      isLightLevel(second_half, bright_level, dark_level);

  if (first_is_light == second_is_light) {
    return false;
  }

  value = first_is_light;
  return true;
}

bool readByte(uint32_t data_started_at, size_t& bit_index,
              uint16_t bright_level, uint16_t dark_level, uint8_t& value) {
  value = 0;
  for (uint8_t bit = 0; bit < 8; ++bit, ++bit_index) {
    bool decoded_bit = false;
    if (!readManchesterBit(data_started_at, bit_index, bright_level, dark_level,
                           decoded_bit)) {
      return false;
    }
    value = static_cast<uint8_t>((value << 1U) | (decoded_bit ? 1U : 0U));
  }
  return true;
}

bool receivePacket(ReceivedPacket& packet) {
  uint32_t frame_started_at = 0;
  uint16_t bright_level = 0;
  uint16_t dark_level = 0;
  if (!detectPreamble(frame_started_at, bright_level, dark_level)) {
    return false;
  }

  const uint32_t data_started_at =
      frame_started_at +
      (lifi_protocol::SYNC_ON_MS + lifi_protocol::CALIBRATION_OFF_MS) * 1000U;
  size_t bit_index = 0;
  uint8_t frame[lifi_protocol::MAX_FRAME_LENGTH] = {};

  if (!readByte(data_started_at, bit_index, bright_level, dark_level, frame[0]) ||
      frame[0] != lifi_protocol::MAGIC ||
      !readByte(data_started_at, bit_index, bright_level, dark_level, frame[1]) ||
      !readByte(data_started_at, bit_index, bright_level, dark_level, frame[2])) {
    Serial.println("Rejected frame: invalid Manchester header or magic byte");
    return false;
  }

  packet.sequence = frame[1];
  packet.length = frame[2];
  if (packet.length == 0 || packet.length > lifi_protocol::MAX_MESSAGE_LENGTH) {
    Serial.println("Rejected frame: invalid payload length");
    return false;
  }

  for (size_t index = 0; index < packet.length; ++index) {
    if (!readByte(data_started_at, bit_index, bright_level, dark_level,
                  frame[lifi_protocol::HEADER_LENGTH + index])) {
      Serial.println("Rejected frame: invalid Manchester payload");
      return false;
    }
    packet.payload[index] =
        static_cast<char>(frame[lifi_protocol::HEADER_LENGTH + index]);
  }
  packet.payload[packet.length] = '\0';

  const size_t bytes_before_crc = lifi_protocol::HEADER_LENGTH + packet.length;
  uint8_t received_crc = 0;
  if (!readByte(data_started_at, bit_index, bright_level, dark_level,
                received_crc)) {
    Serial.println("Rejected frame: invalid Manchester CRC");
    return false;
  }

  const uint8_t calculated_crc = lifi_protocol::crc8(frame, bytes_before_crc);
  if (received_crc != calculated_crc) {
    Serial.printf("Rejected frame: CRC received=%02X calculated=%02X\n",
                  received_crc, calculated_crc);
    return false;
  }

  return true;
}

void showPacket(const ReceivedPacket& packet) {
  if (!display_available) {
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Li-Fi received");
  display.printf("Sequence: %u\n", packet.sequence);
  display.println();
  display.println(packet.payload);
  display.display();
}

void sendAcknowledgement() {
  delay(lifi_protocol::ACK_DELAY_MS);
  for (uint8_t burst = 0; burst < lifi_protocol::IR_BURST_COUNT; ++burst) {
    ledcWrite(IR_PWM_CHANNEL, IR_PWM_DUTY);
    delayMicroseconds(lifi_protocol::IR_BURST_US);
    ledcWrite(IR_PWM_CHANNEL, 0);
    delayMicroseconds(lifi_protocol::IR_GAP_US);
  }
}
}  // namespace

void setup() {
  Serial.begin(lifi_protocol::SERIAL_BAUD);

  pinMode(receiver_pins::IR_TX, OUTPUT);
  digitalWrite(receiver_pins::IR_TX, LOW);
  ledcSetup(IR_PWM_CHANNEL, lifi_protocol::IR_CARRIER_HZ,
            IR_PWM_RESOLUTION_BITS);
  ledcAttachPin(receiver_pins::IR_TX, IR_PWM_CHANNEL);
  ledcWrite(IR_PWM_CHANNEL, 0);

  pinMode(receiver_pins::LIFI_ADC, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(receiver_pins::LIFI_ADC, ADC_11db);

  Wire.begin(receiver_pins::OLED_SDA, receiver_pins::OLED_SCL);
  display_available = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (display_available) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Li-Fi receiver");
    display.println("Waiting for light...");
    display.display();
  }

  delay(500);
  idle_level = readAdcAverage(32);
  Serial.println("Li-Fi receiver ready");
  Serial.printf("OLED: %s | initial ADC idle level: %ld\n",
                display_available ? "ready" : "not detected", idle_level);
}

void loop() {
  ReceivedPacket packet = {};
  if (receivePacket(packet)) {
    const bool duplicate = has_last_sequence && packet.sequence == last_sequence;
    if (duplicate) {
      Serial.printf("Duplicate sequence %u received; ACK repeated\n",
                    packet.sequence);
    } else {
      Serial.printf("Received sequence %u: \"%s\"\n", packet.sequence,
                    packet.payload);
      showPacket(packet);
      last_sequence = packet.sequence;
      has_last_sequence = true;
    }
    sendAcknowledgement();

    // Relearn the idle level after the optical frame and IR switching finish.
    delay(lifi_protocol::FRAME_GUARD_MS);
    idle_level = readAdcAverage(16);
  }

  if (millis() - last_idle_report_ms >= 2000) {
    last_idle_report_ms = millis();
    Serial.printf("Waiting | ADC idle=%ld current=%u\n", idle_level,
                  readAdcAverage(4));
  }
}
