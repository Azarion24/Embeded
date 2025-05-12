# STM32 Program - PWM and UART LED Control

## Overview

This project is a firmware application for STM32L4 microcontrollers, written in C, which provides functionality to control an LED using PWM (Pulse Width Modulation) and UART communication. The program allows the user to:
- Adjust the brightness of the LED (0-100%).
- Configure the LED to blink at a specific frequency (1-100 Hz).

The application is designed to work seamlessly with STM32's low-layer (LL) drivers and is a great example of combining real-time control with user interaction via UART.

---

## Features

1. **PWM Control**:
    - Adjust the brightness of the LED using PWM.
    - Initial brightness is set to 50%.
    
2. **Blinking Mode**:
    - The LED can blink at a configurable frequency (1-100 Hz).
    - When blinking, the LED maintains the configured brightness.

3. **UART Communication**:
    - Users can send commands via UART to control the LED.
    - Supports two commands:
      - `1[value]`: Set brightness of the LED (e.g., `150` sets brightness to 50%).
      - `2[value]`: Set blinking frequency of the LED (e.g., `210` sets blinking frequency to 10 Hz).

4. **Non-blocking Design**:
    - The program uses a main loop to process UART input and LED blinking without delaying other operations.

---

## How It Works

### Main Components
1. **SysTick Timer**:
    - Configured to generate an interrupt every 1 ms.
    - Used to keep track of time and manage LED blinking.

2. **PWM Initialization**:
    - Timer TIM2 is configured for PWM generation on pin `PB3`.
    - The brightness is controlled by adjusting the PWM duty cycle.

3. **UART Initialization**:
    - USART2 is configured for communication.
    - The program listens for user input and processes commands.

4. **LED Blinking**:
    - The program toggles the LED state (ON/OFF) based on the configured blinking frequency.
    - The blinking logic is implemented using timestamps and the SysTick timer.

---

## Usage Instructions

### Hardware Setup
1. Connect an LED to pin `PB3` (configured for PWM output).
2. Connect the UART interface (USART2) to a terminal (e.g., via USB-to-UART converter).
3. Ensure the STM32 microcontroller is powered and properly configured.

### UART Commands
- **Command 1: Set Brightness**
  - Syntax: `1[value]`
  - Example: `150` (Sets LED brightness to 50%).
  - Valid range: `0-100`.

- **Command 2: Set Blinking Frequency**
  - Syntax: `2[value]`
  - Example: `210` (Sets LED blinking frequency to 10 Hz).
  - Valid range: `1-100 Hz`.

### Example Workflow
1. Open a UART terminal (e.g., Tera Term or PuTTY) and connect to the STM32 at `115200` baud rate.
2. After startup, the program sends a welcome message:
   ```
   Witaj w programie na przemiot DSC
   [1] Ustaw jasnosc PWM (0-100%)
   [2] Miganie diody z czestotliwoscia (1-100 Hz)
   ```
3. Send the command `150` to set the LED brightness to 50%.
4. Send the command `210` to make the LED blink at 10 Hz.
5. Observe the LED behavior in real time.

---

## Code Explanation

### Key Functions
- **`process_uart_input`**:
  - Handles UART input from the user.
  - Processes commands to adjust LED brightness or blinking frequency.

- **`LED_SetBrightness`**:
  - Configures the PWM duty cycle to adjust LED brightness.

- **`LED_Blink`**:
  - Toggles the LED state based on the configured blinking frequency.

- **`SysTick_Handler`**:
  - Increments a millisecond counter used for timekeeping.

### File Structure
```plaintext
main.c
```

### Example Code Snippet
```c
void process_uart_input(void) {
    if (LL_USART_IsActiveFlag_RXNE(USART2) || LL_USART_IsActiveFlag_ORE(USART2)) {
        // Handle UART input and process commands
    }
}

void LED_SetBrightness(uint8_t percent) {
    uint32_t arr = LL_TIM_GetAutoReload(TIM2);
    uint32_t ccr = (percent * (arr + 1)) / 100;
    LL_TIM_OC_SetCompareCH2(TIM2, ccr);
}

void LED_Blink(void) {
    static uint32_t last_time = 0;
    static bool led_on = true;
    uint32_t current_time = LL_GetTick();

    if (blink_frequency == 0) return;

    if (current_time - last_time >= (1000 / blink_frequency)) {
        if (led_on) {
            LED_SetBrightness(0);
            led_on = false;
        } else {
            LED_SetBrightness(led_brightness);
            led_on = true;
        }
        last_time = current_time;
    }
}
```

---

## Requirements

- **Hardware**:
  - STM32L4 microcontroller.
  - LED connected to `PB3`.
  - UART terminal (e.g., Tera Term, PuTTY).

- **Software**:
  - STM32CubeIDE or equivalent toolchain.
  - STM32 HAL/LL drivers.

