# LiFi Project

ESP32 firmware for a visible-light transmitter, legitimate receiver, and a
later eavesdropper experiment.

The project uses PlatformIO with the Arduino framework. The initial firmware is
diagnostic-only: transmitter LED outputs are held off, while the receiver prints
raw GPIO34 ADC readings over Serial.

## Environments

- `transmitter`: keypad, white Li-Fi LED, warning LED, and IR ACK receiver
- `receiver`: BPW34/TIA ADC input, OLED, and IR ACK transmitter
- `eavesdropper`: reserved until the legitimate optical link is verified

## Basic commands

```bash
pio run -e transmitter
pio run -e receiver
pio run -e transmitter -t upload
pio run -e receiver -t upload
pio device monitor --baud 115200
```
