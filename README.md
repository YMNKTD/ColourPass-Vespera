
<p align="center">
  <img src="Images/colourpass-icon.png" alt="ColourPass Logo" width="250">
</p>

# ColourPass – An Interactive Lighting Controller

## Project Overview

ColourPass is an Arduino-based physical interface designed to control the Vespera light installation through colour recognition, touch interaction, and MQTT.

Inspired by ticket validation systems, the project explores how physical objects can become interactive interfaces for connected environments, allowing users to influence lighting through simple and intuitive actions.

## Interaction Design

### 1. Colour-Based Ticket Validation

Users present coloured cards to a colour sensor to access different lighting experiences.

- **Red, Orange, Pink:** Access denied. The lights flash red.
- **Yellow, Green:** Access granted. Enter Static Mode.
- **Blue, Purple:** Access granted. Enter Ambient Mode.
- **Black or Unrecognised Cards:** No mode is activated.

Successful validation triggers a green light for two seconds before the selected mode begins.

### 2. Static Mode

Yellow and green cards activate Static Mode.

The lights initially display the colour of the scanned card. Users can then touch the sensor to cycle through different solid colours.

### 3. Ambient Mode

Blue and purple cards activate Ambient Mode.

In this mode, users can explore different dynamic lighting effects through touch interaction. Each touch switches to the next lighting effect, allowing users to personalise their experience.

The lighting effects will be designed, tested, and refined throughout the prototyping process.

### 4. Touch Controls and Reset

The touch sensor supports two types of interaction:

- **Single Tap:** Switches to the next colour or dynamic lighting effect.
- **Long Press (3 seconds):** Exits the current lighting mode and returns to the card scanning stage.

A brief white light flash provides visual confirmation of a successful reset.

The physical switch is intended for powering the device on and off.

## Hardware

- Arduino MKR WiFi 1010
- DFRobot Colour Sensor
- Grove Touch Sensor
- Physical Power Switch
- Vespera Light Installation

## Communication

The Arduino will connect to the network via Wi-Fi and use MQTT to send lighting control data to the Vespera installation.

## Development Status

This project is currently in the planning and prototyping stage. The proposed functionality will be implemented and evaluated through iterative development.

Source code, circuit diagrams, testing results, and prototype photographs will be added as development progresses.


---
*Designed by Monong Yang (Morgan)*
