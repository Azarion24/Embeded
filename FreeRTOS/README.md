# STM32 FreeRTOS ADC Monitoring System

This project implements a multitasking analog signal monitoring and analysis system for STM32 microcontrollers using FreeRTOS. The application samples data from the ADC, provides statistical analysis, and generates alerts, all accessible via UART commands.

---

## Features

- **UART Command Interface**: Interactive text-based command parser for user control.
- **LED Control**: Start/stop LED blinking as a visual system indicator.
- **ADC Sampling**: Periodic sampling of the internal temperature sensor.
- **Statistical Analysis**: Real-time calculation of average, maximum, or minimum values from ADC samples.
- **Alert System**: Threshold-based alerting with edge detection to avoid repeated notifications.
- **Multitasking**: Five FreeRTOS tasks for modular and responsive operation.
- **Configurable Parameters**: Adjust sampling period, analysis mode, sample count, and alert threshold at runtime.

---

## System Architecture

### FreeRTOS Tasks

| Task Name      | Functionality                              | Priority         | Stack Size |
|----------------|--------------------------------------------|------------------|------------|
| UartTask       | UART command parsing and user interaction  | Normal           | 1024 bytes |
| LedTask        | LED blinking control                       | Low              | 512 bytes  |
| ADCSampler     | ADC data acquisition                       | Low              | 1024 bytes |
| DataAnalyzer   | Statistical analysis (AVG/MAX/MIN)         | Low              | 1024 bytes |
| AlertMonitor   | Alert generation based on ADC value        | Low              | 1024 bytes |

### Inter-task Communication

- **Message Queues**:
  - `ledCmd`: Commands for LED control
  - `adcAnalyzer`: ADC samples for analysis
  - `adcAlert`: ADC samples for alert monitoring
  - `uartTx`: UART RX interrupt buffer

---

## UART Command Reference

Interact with the system via UART (9600 baud, 8N1). Supported commands:

### LED Control
- `START LED` — Start blinking the LED
- `STOP LED` — Stop blinking the LED

### ADC Management
- `GET ADC` — Read and display a single ADC value
- `SET PERIOD <ms>` — Set ADC sampling period (10–5000 ms)
- `GET PERIOD` — Show current sampling period

### Alert System
- `SET ALERT <value>` — Set alert threshold
- `GET ALERT` — Show current alert threshold

### Data Analysis
- `START ANALYSIS` — Start analysis (default: AVG mode)
- `STOP ANALYSIS` — Stop analysis
- `SET ANALYSIS AVG|MAX|MIN` — Select analysis mode
- `GET ANALYSIS` — Show current analysis mode
- `SET SAMPLES <value>` — Set number of samples for analysis (1–100)

### System
- `HELP` — Show help and command list
- `EXIT` — Stop analysis and turn off LED

---

## Default Parameters

- **ADC sampling period**: 1000 ms
- **Alert threshold**: 1500
- **Sample count for analysis**: 10
- **Analysis mode**: OFF

---

## Example UART Session

```START LED
> START LED
LED blinking started
> SET PERIOD 500
ADC period set to 500ms
> START ANALYSIS
Data analysis started (AVG mode)
> SET ALERT 1800
Alert threshold set to 1800
> 
AVG: 1654
> 
AVG: 1672
> 
!!! ALERT: Value 1820 above limit !!!
>
```
---

## Hardware & Configuration

- **Microcontroller**: STM32 (with ADC and UART)
- **ADC**: 12-bit, internal temperature sensor, 247.5 cycles sampling time
- **UART**: USART2, 9600 baud, 8N1, no flow control
- **LED**: Connected to user GPIO
- **Development**: STM32CubeIDE, HAL drivers, FreeRTOS

---

## Code Structure

- `main.c` — System initialization, FreeRTOS setup, peripheral configuration
- **Tasks**:
  - `StartUartTask` — Handles UART RX, command parsing, and response
  - `StartLedTask` — Controls LED blinking based on commands
  - `StartADCSampler` — Periodically samples ADC and distributes data
  - `StartDataAnalyzer` — Buffers samples and computes AVG/MAX/MIN
  - `StartAlertMonitor` — Monitors samples for threshold violations and sends alerts
- **Command Handler**: Parses and executes user commands, manages system state

---


