# STM32 Vehicle Tracker – How It Works

This project is a **vehicle monitoring and tracking system** based on an STM32 microcontroller, integrating a 3-axis accelerometer (LIS3DH), GPS module, GSM modem (SIM800L), AWS-based HTTP proxy, and Firebase database. The firmware is written in C and designed for real-time operation in embedded environments.  
A dedicated mobile application has also been developed to provide users with real-time access to vehicle status and alerts.

---

## System Architecture

| Component           | Functionality                                                                 |
|---------------------|-------------------------------------------------------------------------------|
| STM32 MCU           | Central control, data processing, peripheral management                       |
| LIS3DH (SPI)        | Detects motion, inactivity, and possible theft via 3-axis acceleration        |
| GPS (UART)          | Provides current location, speed, and altitude                                |
| SIM800L (UART)      | Sends JSON data via HTTP to AWS proxy server                                  |
| AWS HTTP Proxy      | Receives HTTP (non-SSL) requests from SIM800L and relays them as HTTPS to Firebase |
| Firebase            | Stores vehicle status, location, trip history, and parking events             |
| Dedicated App       | Displays live data, trip history, alerts, and theft notifications to the user |
| Dual Li-Ion Power   | Two 3.7V 2200mAh Li-Ion cells in series, with BMS/charger for backup power    |

---

## Why AWS Proxy?

The **SIM800L module does not support HTTPS connections** natively, which are required by most modern cloud services, including Firebase. To solve this, the system uses a lightweight **HTTP proxy hosted on AWS** (e.g., as a PHP script or Lambda function behind API Gateway). The STM32 sends data via HTTP to AWS, which then securely forwards it to Firebase over HTTPS. This ensures compatibility and security, despite hardware limitations.

---

## Main Features

- **Motion Detection:**  
  The LIS3DH accelerometer continuously measures acceleration in all axes. If a significant change (above a set threshold) is detected, the system recognizes the vehicle as moving. If no movement is detected for a defined timeout, the system switches to "stopped" state.

- **GPS Location Tracking:**  
  The GPS module provides real-time latitude, longitude, speed, and altitude. Data is parsed from NMEA sentences, validated, and formatted for transmission.

- **Event-driven Reporting:**  
  - **Trip Start:** When motion is detected, the system logs the trip start location and time.
  - **Trip End:** When the vehicle stops, the trip end is recorded.
  - **Last Parking:** After stopping, the last parking location is saved.
  - **Current Location:** While moving, the system periodically sends the current location.

- **GSM & Cloud Communication:**  
  The SIM800L modem establishes a GPRS connection and sends HTTP POST requests with JSON payloads to the AWS proxy. The proxy then relays the data to Firebase over HTTPS.

- **Robustness:**  
  - All communication is event-driven and protected against data loss.
  - The system initializes and closes the GSM connection as needed to save power.

- **Dedicated Mobile Application:**  
  A custom mobile app enables users to monitor their vehicle in real time, view trip history, and receive alerts about suspicious activity or theft.

---

## Emergency & Backup Power

The device is powered by **two 3.7V 2200mAh Li-Ion cells connected in series** (for a total of 7.4V nominal), providing extended backup operation. The pack is integrated with a **Battery Management System (BMS)** and a charging module, which ensures:
- Safe charging and discharging
- Protection against overcurrent, overvoltage, undervoltage, and short circuits
- Reliable operation during power outages or when the vehicle is off

---

## Planned Features

- **Emergency RF Communication (433 MHz):**  
  In the event of GSM and GPS jamming (detected by the system), an emergency communication channel using RF 433 MHz will be activated.  
  - The STM32 will transmit an alert via RF.
  - A receiver based on ESP (e.g., ESP32) will receive the RF signal and update the vehicle status in Firebase.
  - This ensures that the user is notified in the mobile app about a possible theft or tampering, even if cellular and GPS signals are blocked[1][3][5].

---

## Data Flow

1. **Initialization:**  
   - Peripherals (SPI, UART, GPIO) are initialized.
   - LIS3DH is configured for ±2g range and 10Hz sampling.
   - GPS UART is set up for NMEA data reception.
2. **Main Loop:**  
   - The accelerometer is sampled every 100 ms.
   - If motion is detected, the system:
     - Initializes GSM (if not already done)
     - Sends "trip begin" and "current status" to Firebase via AWS proxy
     - Starts periodic location updates
   - If inactivity is detected for a set timeout:
     - Sends "trip end", "last parking", and "current status" (stopped)
     - Closes GSM connection
   - GPS data is parsed and validated in UART interrupt callbacks.
3. **Data Transmission:**  
   - JSON data is constructed with current values and sent via SIM800L using HTTP (non-SSL) to AWS proxy.
   - The AWS proxy receives HTTP data and forwards it as HTTPS to Firebase.
   - In the future, emergency RF alerts will be sent to an ESP receiver for critical updates.

---

## Example JSON Payloads

```
// Current Location
{
"type": "currentLocation",
"lat": 52.2297,
"lng": 21.0122,
"time": "14:32:10 03.07.2025",
"speed": 34.2,
"altitude": 120.5
}

// Trip Begin
{
"type": "tripBegin",
"lat": 52.2297,
"lng": 21.0122,
"time": "14:32:10 03.07.2025",
"altitude": 120.5
}
```


---

## Hardware Connections

- **LIS3DH Accelerometer:** SPI interface, chip select on dedicated GPIO
- **GPS Module:** UART1 (9600 baud)
- **SIM800L GSM Modem:** UART3 (9600 baud)
- **AWS Proxy:** Public HTTP endpoint
- **Dual Li-Ion Pack:** Two 3.7V 2200mAh cells in series, with BMS/charger
- **(Planned) RF 433 MHz Transmitter:** For emergency communication with ESP receiver

---

## Summary

This firmware provides a **robust, low-power vehicle tracker** that autonomously detects trips, logs parking and movement, and reliably reports all events and locations to a cloud backend via cellular network.  
The use of an **AWS HTTP proxy** solves the HTTPS limitation of SIM800L, while the **dual-cell Li-Ion pack with BMS** ensures reliable operation even in case of power loss.  
**Planned RF 433 MHz emergency communication** will further enhance security by allowing theft alerts even when GSM and GPS are jammed.  
A dedicated mobile application gives users full insight and real-time notifications about their vehicle's status and security.
