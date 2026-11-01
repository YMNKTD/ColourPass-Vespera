# ColourPass — Source Code

## 1. Overview

This folder contains the Arduino source code for **ColourPass v2.0**, an interactive colour-based ticket validation and lighting control system developed for UCL CASA0014.

The program uses an Arduino MKR WiFi 1010 to recognise coloured tickets, process touch interactions and control Vespera lighting through Wi-Fi and MQTT.

## 2. File Structure

| File | Description |
|---|---|
| `ColourPass_Vespera_v2.0.ino` | Main program and system coordination |
| `Config.h` | Hardware pins, timing and MQTT settings |
| `ColourModels.h` | Colour and system state definitions |
| `ColourSensor.h / .cpp` | Colour recognition and calibration |
| `TouchInput.h / .cpp` | Touch detection and long-press controls |
| `LCDView.h / .cpp` | LCD display and status feedback |
| `LightingEngine.h / .cpp` | Static and ambient lighting effects |
| `VesperaClient.h / .cpp` | Wi-Fi and MQTT communication |
| `arduino_secrets.h.example.h` | Example network credentials |
| `ColourPass_Vespera_V2_Sample.zip` | Archived source code package |

## 3. Main Features

- Nine predefined colour calibration profiles.
- Automatic background measurement at startup.
- Continuous colour-based ticket validation.
- Six static colours and six ambient lighting themes.
- Touch controls with 3-second and 5-second long-press functions.
- LCD feedback for validation, lighting modes and network status.
- Automatic Wi-Fi/MQTT reconnection.
- Local NeoPixel preview and remote Vespera lighting control.

## 4. Setup

1. Install Arduino IDE and select **Arduino MKR WiFi 1010**.
2. Install the required libraries: `WiFiNINA`, `PubSubClient`, `Adafruit NeoPixel` and `Waveshare_LCD1602_RGB`.
3. Place all source files in one Arduino sketch folder, ensuring the folder name matches the main `.ino` filename.
4. Create `arduino_secrets.h` using the provided example and enter your network credentials.
5. Configure the MQTT host, port and topic in `Config.h`.
6. Verify and upload the program using Arduino IDE.

Open the Serial Monitor at **115200 baud** for debugging.

The published source requires valid network configuration and an authorised MQTT endpoint. Credentials are excluded from GitHub.

## 5. Development Records

For earlier code versions, testing results and technical improvements, see:

- [Part 02 — Hardware Integration and Colour Calibration](../design-log/part-02-hardware-integration-and-colour-calibration/README.md)
- [Part 03 — MQTT Integration and Lighting Control](../design-log/part-03-mqtt-integration-and-lighting-control/README.md)
- [Part 05 — Colour Recognition and Code Refinement](../design-log/part-05-colour-recognition-and-code-refinement/README.md)
