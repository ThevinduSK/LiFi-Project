# ESP32 Li-Fi Communication and Security Demonstrator

This repository contains the firmware and design files for a three-node
visible-light communication experiment:

1. A **transmitter** sends short messages by switching a high-power white LED.
2. A **legitimate receiver** detects the light with a BPW34 photodiode and
   displays valid messages on an OLED.
3. An **eavesdropper receiver** will later be used to measure how much of the
   optical signal can be recovered from an unintended position.

The current firmware is a compiled proof-of-concept for the first two nodes.
It uses on-off keying (OOK), Manchester line coding, framed messages, CRC-8
error detection, and a 38 kHz infrared acknowledgment path. It has not yet
been validated on the assembled PCBs.

For the complete design, security scope, implementation status, and test plan,
see [Project Technical Design and Status](docs/PROJECT_TECHNICAL_DESIGN_AND_STATUS.md).

## Current status

| Area | Status |
|---|---|
| PlatformIO ESP32 project | Implemented |
| Transmitter and receiver pin definitions | Implemented from supplied schematics |
| OOK and Manchester transmitter | Implemented and compiles |
| ADC Manchester receiver | Implemented and compiles |
| Message framing and CRC-8 | Implemented and compiles |
| OLED message display | Implemented and compiles |
| 38 kHz IR acknowledgment | Implemented and compiles |
| Separate hardware diagnostic firmware | Planned next |
| On-demand `A` link-quality handshake | Planned next |
| Simple `B` normal-message mode | Planned next |
| SNR and known-sequence success measurement | Planned next |
| Physical PCB testing and tuning | Not started |
| Eavesdropper experiment | Deferred until the legitimate link is stable |
| Encryption/authentication | Not implemented; optional later scope |

“Compiles” means PlatformIO successfully built the firmware. It does not mean
that the code has been uploaded to a board or that the optical thresholds and
timing have been proven on the physical link.

## Intended user workflow

The next firmware revision will use a deliberately small interface:

- Press `A` to run a link test. The transmitter sends a known optical sequence.
  The receiver estimates optical SNR and bit success rate, displays the result,
  and returns an IR result to the transmitter.
- Press `B` to enter normal message mode. Enter a short message, use `*` to
  erase, and press `#` to transmit.
- Normal messages will retain a short start marker and CRC, but will not repeat
  the full training/calibration sequence or wait for an IR ACK after every
  message.
- Run `A` again when the distance, alignment, ambient lighting, or LED level
  changes significantly.

The code currently in `src/transmitter` and `src/receiver` is the earlier full
prototype: it performs optical calibration and waits for an IR ACK on every
message. That behavior is documented, but will be simplified in the next
implementation stage.

## Hardware pin map

The pin assignments below match the supplied transmitter and receiver
schematics.

### Transmitter ESP32

| Function | GPIO |
|---|---:|
| White LED MOSFET gate | 18 |
| Warning/status LED | 23 |
| IR acknowledgment receiver | 19 |
| Keypad rows R1-R4 | 13, 14, 27, 26 |
| Keypad columns C1-C4 | 25, 33, 32, 16 |

### Legitimate receiver ESP32

| Function | GPIO |
|---|---:|
| BPW34/LM358 analog output | 34 |
| IR acknowledgment transmitter | 18 |
| OLED SDA | 21 |
| OLED SCL | 22 |

The transmitter firmware starts the high-power LED output in the OFF state.
LED current is determined by the external LED, supply, resistor, MOSFET, and
thermal design—not by the ESP32 firmware.

## Repository layout

```text
.
├── docs/                         Detailed design and progress document
├── include/
│   ├── lifi_protocol.hpp         Protocol constants and CRC-8
│   └── pin_config.hpp            Schematic-derived ESP32 pin assignments
├── PCB/
│   ├── Transmitter/              Transmitter schematic and fabrication files
│   └── Receiver/                 Receiver schematic and fabrication files
├── src/
│   ├── transmitter/main.cpp      Current transmitter proof-of-concept
│   ├── receiver/main.cpp         Current receiver proof-of-concept
│   └── eavesdropper/main.cpp     Placeholder for the later experiment
└── platformio.ini                PlatformIO environments and dependencies
```

## Building with PlatformIO

Open this repository as the project folder in VS Code. PlatformIO should show
the `transmitter`, `receiver`, and `eavesdropper` environments.

Build the two active nodes:

```bash
pio run -e transmitter
pio run -e receiver
```

Upload one environment after connecting the correct ESP32:

```bash
pio run -e transmitter -t upload
pio run -e receiver -t upload
```

Open the serial monitor at 115200 baud:

```bash
pio device monitor --baud 115200
```

Do not connect both ESP32 boards at once unless the upload port is selected
explicitly. Before powering the high-power LED stage, verify its supply,
polarity, current-limiting component, and thermal requirements.

## Current proof-of-concept controls

These controls describe the code presently in the repository, before the
planned `A`/`B` mode separation:

- `0-9` and `A-D`: append a character.
- `*`: erase the last character.
- `#`: transmit the message.
- Serial Monitor input follows the same rules; newline also transmits.
- Maximum message length: 16 characters.

## Important security statement

This is a **Li-Fi security demonstrator**, not yet a secure messaging product.
Directional visible light may reduce casual signal exposure, but it does not
guarantee confidentiality. CRC detects accidental corruption; it does not
authenticate a sender or prevent deliberate modification. The IR ACK proves
only that an IR response was detected, not that no passive eavesdropper was
present.

The security contribution will be the measured comparison between the
legitimate receiver and an eavesdropper under controlled placement, angle,
distance, obstruction, and ambient-light conditions. Cryptographic protection
can be added later if it becomes part of the project scope.
