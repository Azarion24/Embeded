# STM32L4xx Clock System Control via UART

This project provides a command-line interface over UART for configuring and monitoring the clock system of an STM32L432KC microcontroller. The code allows dynamic switching between different clock sources, configuring the Master Clock Output (MCO), calibrating the MSI oscillator using the LSE, and querying system status—all from a serial terminal.

---

## Features

- **UART Command Interface**  
  Communicate with the MCU via UART (115200 baud) and issue commands to control the clock tree.

- **Clock Source Selection**  
  - Switch system clock between MSI (Multi-Speed Internal), HSI (High-Speed Internal), and PLL (Phase-Locked Loop).
  - Set MSI frequency (4/8/16/24/48 MHz).
  - Enable and check the status of LSE (Low-Speed External) 32.768 kHz oscillator.

- **MCO (Master Clock Output) Configuration**  
  Output selected clock sources (SYSCLK, HSI, MSI, PLL, LSE) on the MCO pin (PA8) with configurable prescaler.

- **MSI Calibration**  
  Calibrate the MSI oscillator using the LSE for improved accuracy.

- **Status Reporting**  
  Query current clock configuration, MCO settings, flash latency, and LSE status.

- **Help System**  
  Built-in help command listing all available commands and usage examples.

---

## Hardware Requirements

- STM32L432KC microcontroller (or compatible STM32L4xx)
- 32.768 kHz crystal for LSE (optional, for calibration)
- UART-USB converter or ST-Link for serial communication
- PA2 (TX) and PA15 (RX) connected to your serial terminal
- PA8 available for MCO output (connect to oscilloscope or frequency counter for verification)

---

## UART Command Set

| Command                | Description                                                        |
|------------------------|--------------------------------------------------------------------|
| `help`                 | Show help and available commands                                   |
| `msi[4|8|16|24|48]`    | Set MSI frequency in MHz (e.g., `msi16` for 16 MHz)                |
| `hsi`                  | Switch SYSCLK to HSI (16 MHz)                                     |
| `pll`                  | Switch SYSCLK to PLL (80 MHz, source: HSI)                        |
| `lseon`                | Enable LSE (32.768 kHz crystal)                                   |
| `calibrate`            | Calibrate MSI using LSE                                            |
| `mco[off|sys|hsi|msi|pll|lse][1|2|4]` | Configure MCO output source and prescaler (e.g., `mcosys2` for SYSCLK/2) |
| `status`               | Show current clock and system status                               |

**Examples:**
- `msi48` — Set MSI to 48 MHz and switch SYSCLK to MSI
- `pll` — Switch system clock to PLL (80 MHz)
- `mcooff` — Disable MCO output
- `mcosys2` — Output SYSCLK divided by 2 on PA8
- `calibrate` — Calibrate MSI oscillator using LSE

---

## Typical UART Session

```
==== STM32L432KC Clock System ====
Type 'help' to see available commands

    msi16
    SYSCLK: 16 MHz (MSI)
    lseon
    LSE ready (32.768 kHz)
    calibrate
    Calibrating MSI...
    MSI calibrated! Accuracy: +-0.25%
    pll
    SYSCLK: 80 MHz (PLL)
    mcosys4
    MCO: sys /4
    status

--- Status ---
SYSCLK: 80 MHz
MCO: sys /4
Flash latency: 4 WS
LSE: active
```

---

## Code Structure

- **main.c**  
  - Peripheral initialization (UART, MCO, LSE)
  - UART RX buffer and command parsing
  - Command processing and system reconfiguration
  - Helper functions for flash latency, MCO, and calibration

- **Key Functions**
  - `UART_Init()` — Configures USART2 (PA2: TX, PA15: RX, 115200 baud)
  - `MCO_Init()` — Sets up PA8 for clock output
  - `LSE_Init()` — Enables and waits for LSE oscillator
  - `SetMCO()` — Selects MCO source and prescaler
  - `SetFlashLatency()` — Adjusts flash wait states for current frequency
  - `CalibrateMSI()` — Trims MSI using LSE reference
  - `processCommand()` — Parses and executes user commands

---

## Building and Running

1. Open the project in STM32CubeIDE or your preferred STM32 development environment.
2. Connect your board via ST-Link or compatible programmer.
3. Build and flash the firmware.
4. Open a serial terminal (e.g., PuTTY, Tera Term) at 115200 baud, 8N1, no flow control.
5. Interact with the system using the commands above.

---

## Notes

- If LSE is not present or fails to start, calibration and related features will not be available.
- MCO output can be observed on PA8 with an oscilloscope or frequency counter.
- All commands are case-insensitive.

