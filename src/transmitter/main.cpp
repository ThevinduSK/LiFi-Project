#include <Arduino.h>
#include <Keypad.h>

#include "lifi_protocol.hpp"
#include "pin_config.hpp"

namespace {
constexpr byte KEYPAD_ROW_COUNT = 4;
constexpr byte KEYPAD_COLUMN_COUNT = 4;

char keypad_map[KEYPAD_ROW_COUNT][KEYPAD_COLUMN_COUNT] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};

byte keypad_row_pins[KEYPAD_ROW_COUNT] = {
    transmitter_pins::KEYPAD_ROWS[0], transmitter_pins::KEYPAD_ROWS[1],
    transmitter_pins::KEYPAD_ROWS[2], transmitter_pins::KEYPAD_ROWS[3]};
byte keypad_column_pins[KEYPAD_COLUMN_COUNT] = {
    transmitter_pins::KEYPAD_COLUMNS[0], transmitter_pins::KEYPAD_COLUMNS[1],
    transmitter_pins::KEYPAD_COLUMNS[2], transmitter_pins::KEYPAD_COLUMNS[3]};

Keypad keypad(makeKeymap(keypad_map), keypad_row_pins, keypad_column_pins,
              KEYPAD_ROW_COUNT, KEYPAD_COLUMN_COUNT);

char message[lifi_protocol::MAX_MESSAGE_LENGTH + 1] = {};
size_t message_length = 0;
uint8_t sequence_number = 0;

void setOpticalLed(bool enabled) {
  digitalWrite(transmitter_pins::LED_GATE, enabled ? HIGH : LOW);
}

void sendManchesterBit(bool value) {
  setOpticalLed(value);
  delayMicroseconds(lifi_protocol::HALF_BIT_US);
  setOpticalLed(!value);
  delayMicroseconds(lifi_protocol::HALF_BIT_US);
}

void sendByte(uint8_t value) {
  for (int8_t bit = 7; bit >= 0; --bit) {
    sendManchesterBit((value & (1U << bit)) != 0U);
  }
}

void sendOpticalFrame(const char* payload, size_t payload_length,
                      uint8_t sequence) {
  uint8_t frame[lifi_protocol::MAX_FRAME_LENGTH] = {};
  frame[0] = lifi_protocol::MAGIC;
  frame[1] = sequence;
  frame[2] = static_cast<uint8_t>(payload_length);

  for (size_t index = 0; index < payload_length; ++index) {
    frame[lifi_protocol::HEADER_LENGTH + index] =
        static_cast<uint8_t>(payload[index]);
  }

  const size_t bytes_before_crc = lifi_protocol::HEADER_LENGTH + payload_length;
  frame[bytes_before_crc] = lifi_protocol::crc8(frame, bytes_before_crc);

  setOpticalLed(false);
  delay(lifi_protocol::IDLE_BEFORE_FRAME_MS);
  setOpticalLed(true);
  delay(lifi_protocol::SYNC_ON_MS);
  setOpticalLed(false);
  delay(lifi_protocol::CALIBRATION_OFF_MS);

  for (size_t index = 0; index <= bytes_before_crc; ++index) {
    sendByte(frame[index]);
  }

  setOpticalLed(false);
  delay(lifi_protocol::FRAME_GUARD_MS);
}

bool waitForAcknowledgement() {
  const uint32_t started_at = millis();
  uint32_t low_started_at = 0;

  while (millis() - started_at < lifi_protocol::ACK_TIMEOUT_MS) {
    if (digitalRead(transmitter_pins::IR_ACK) == LOW) {
      if (low_started_at == 0) {
        low_started_at = micros();
      } else if (micros() - low_started_at >= 150) {
        return true;
      }
    } else {
      low_started_at = 0;
    }
    delayMicroseconds(50);
  }
  return false;
}

void printMessageBuffer() {
  Serial.print("Message: \"");
  Serial.print(message);
  Serial.println("\"");
}

void transmitMessage() {
  if (message_length == 0) {
    Serial.println("Nothing to send. Enter a message first.");
    return;
  }

  Serial.printf("Sending sequence %u: \"%s\"\n", sequence_number, message);
  sendOpticalFrame(message, message_length, sequence_number);

  const bool acknowledged = waitForAcknowledgement();
  digitalWrite(transmitter_pins::WARNING_LED, acknowledged ? LOW : HIGH);
  Serial.println(acknowledged ? "ACK received" : "ACK timeout (link failure)");

  if (acknowledged) {
    ++sequence_number;
    message_length = 0;
    message[0] = '\0';
  }
}

void handleInputCharacter(char input) {
  if (input == '#') {
    transmitMessage();
    return;
  }

  if (input == '*') {
    if (message_length > 0) {
      message[--message_length] = '\0';
    }
    printMessageBuffer();
    return;
  }

  if (input < 32 || input > 126) {
    return;
  }

  if (message_length >= lifi_protocol::MAX_MESSAGE_LENGTH) {
    Serial.println("Message is full; press # to send or * to erase.");
    return;
  }

  message[message_length++] = input;
  message[message_length] = '\0';
  printMessageBuffer();
}
}  // namespace

void setup() {
  Serial.begin(lifi_protocol::SERIAL_BAUD);

  pinMode(transmitter_pins::LED_GATE, OUTPUT);
  pinMode(transmitter_pins::WARNING_LED, OUTPUT);
  pinMode(transmitter_pins::IR_ACK, INPUT_PULLUP);

  // Keep both LEDs off until an explicit hardware test is performed.
  digitalWrite(transmitter_pins::LED_GATE, LOW);
  digitalWrite(transmitter_pins::WARNING_LED, LOW);

  delay(500);
  Serial.println("Li-Fi transmitter ready");
  Serial.println("Keypad/Serial: type text, * erases, # sends (maximum 16 chars)");
  printMessageBuffer();
}

void loop() {
  const char keypad_key = keypad.getKey();
  if (keypad_key != NO_KEY) {
    Serial.printf("Keypad: %c\n", keypad_key);
    handleInputCharacter(keypad_key);
  }

  while (Serial.available() > 0) {
    const char serial_character = static_cast<char>(Serial.read());
    if (serial_character == '\r') {
      continue;
    }
    handleInputCharacter(serial_character == '\n' ? '#' : serial_character);
  }

  delay(2);
}
