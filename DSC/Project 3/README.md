# STM32L4 Signal Processing Demo

**An advanced real-time signal processing example for the STM32L4 microcontroller, featuring ADC, DAC, DMA, UART, and CMSIS-DSP integration.**

---

## Project Overview

This project demonstrates a **real-time signal processing chain** on the STM32L4 microcontroller, providing:

- Generation of a sine wave signal with adjustable frequency.
- Sampling of analog input via **ADC** and playback via **DAC**.
- Real-time DSP processing, including:
  - Adjustable gain
  - Optional low-pass FIR filtering (32 taps)
- **Ping-pong buffering** using **DMA** for minimal latency.
- Control and diagnostics via **UART** (text commands).

The project utilizes the **CMSIS-DSP library** for signal operations (scaling, FIR filtering)[1].

---

## Main Features

- **Sine wave generator** (10 Hz – 10 kHz, 12-bit resolution)
- **DSP processing**: gain, FIR filter (enable/disable)
- **Ping-pong buffering** for ADC/DAC (seamless sampling)
- **UART control interface** (115200 baud, text commands)
- **System status** and diagnostics via UART

---

## UART Command Reference

Commands are entered via a UART terminal (e.g., PuTTY, RealTerm):

| Command           | Description                                              |
|-------------------|---------------------------------------------------------|
| `help`            | Show available commands                                 |
| `genon`           | Enable signal generator                                 |
| `genoff`          | Disable signal generator                                |
| `freq <Hz>`       | Set generator frequency (10–10000 Hz)                   |
| `start`           | Start ADC/DAC signal chain                              |
| `stop`            | Stop ADC/DAC signal chain                               |
| `dsp on`/`off`    | Enable/disable DSP processing                           |
| `gain <x>`        | Set gain (0.1–10.0)                                     |
| `filter on`/`off` | Enable/disable FIR filter                               |
| `status`          | Show current system status                              |

---

## Hardware Requirements

- **STM32L4 board** (e.g., STM32L476 Discovery/Nucleo)
- Connections:
  - Analog input: PA0 (ADC)
  - Generator output: PA4 (DAC1 Ch1)
  - Processed output: PA5 (DAC1 Ch2)
  - UART: PA2 (TX), PA15 (RX)

---

## Dependencies

- **STM32L4 HAL/LL drivers**
- **CMSIS-DSP** (included in the project)
- ARM toolchain (e.g., STM32CubeIDE, arm
