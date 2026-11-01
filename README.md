 
<p align="center">
  <img src="Images/colourpass-icon.png" alt="ColourPass Logo" width="220">
</p>

# ColourPass

## 1. Project Overview

**ColourPass** is an IoT prototype that transforms colour recognition into a physical interface for controlling the Vespera lighting.

Inspired by ticket validation systems used in public transport and football event venues, the project explores how simple physical interactions can be translated into different lighting experiences within a connected environment.

Users present a coloured card to the device. An Arduino MKR WiFi 1010 processes the colour sensor readings, determines the validation outcome and communicates lighting instructions to Vespera through Wi-Fi and MQTT. A touch sensor allows users to switch between lighting effects, while an LCD screen provides immediate feedback.

The prototype combines physical interaction, sensor-based decision-making, embedded programming and networked lighting control.

<!-- Physical Prototype -->
<p align="center">
  <img src="design-log/part-04-physical-prototype-construction/ColourPass_Assembled_Physical_Prototype.jpeg" alt="ColourPass Physical Prototype" width="420">
</p>

## 2. Initial Concept and Design

ColourPass began with hand-drawn sketches exploring how coloured tickets could be used as a physical interface for controlling the Vespera lighting installation.

The initial concept introduced colour-based ticket validation, static and ambient lighting modes, and touch interaction. The physical design was subsequently developed to accommodate the electronic components and improve usability.

### Initial System Logic

<!-- Initial System Logic -->
<p align="center">
  <img src="Images/initial-concept-and-system-logic.jpg" alt="Initial System Logic" width="350">
</p>

### Initial Physical Prototype Design

<!-- Initial Physical Prototype -->
<p align="center">
  <img src="Images/initial-physical-prototype-sketch.jpg" alt="Initial Physical Prototype Design" width="400">
</p>

These sketches provided a starting point for development. The design was subsequently modified in response to hardware integration, colour-recognition testing and practical prototyping constraints.

## 3. How ColourPass Works

The interaction follows a sequence:

**Colour Detection → Ticket Validation → Lighting Feedback → Mode Selection → Touch Interaction**

### Ticket Validation

| Ticket Colour | Validation | System Response |
|---|---|---|
| Red | Denied | Red flashing light |
| Orange | Denied | Red flashing light |
| Pink | Denied | Red flashing light |
| Yellow | Granted | Static Mode – Yellow |
| Green | Granted | Static Mode – Green |
| Blue | Granted | Ambient Mode – Blue Mood |
| Purple | Granted | Ambient Mode – Rainbow |

When access is granted, a two-second animated green-and-white confirmation is displayed before entering the appropriate lighting mode.

Rejected tickets produce a red flashing response before the system returns to white standby lighting.

ColourPass also supports continuous ticket recognition, allowing different tickets to be presented without restarting the controller.

### Static Lighting Mode

Yellow and green tickets activate Static Mode.

The initial lighting colour matches the validated ticket. Each short touch advances through six solid colours:

**Yellow → Green → Cyan → Blue → Purple → Pink → Repeat**

### Ambient Lighting Mode

Blue and purple tickets activate Ambient Mode.

6 distinct lighting themes are implemented:

| Theme | Lighting Effect |
|---|---|
| Blue Mood | Animated blue-purple gradient |
| Rainbow | Multicolour rainbow pattern |
| Forest | Animated green-toned lighting |
| Sunset | Animated warm-colour gradient |
| Disco | Dynamic multicolour lighting |
| Bayern | Alternating red and white lighting |

Blue tickets initially activate Blue Mood, while purple tickets activate Rainbow.

The touch sequence contains seven positions, with Blue Mood appearing twice, providing different starting points for the two ticket colours.

### Touch Interaction

| Interaction | Function |
|---|---|
| Single Tap | Change the current colour or lighting theme |
| Hold for 3 seconds | Display a background-recheck prompt |
| Release after 3–5 seconds | Recheck the empty scanning background |
| Hold for 5 seconds | Display a full-reset prompt |
| Release after 5 seconds | Restart the controller and reload colour profiles |

The LCD displays the long-press prompts as their respective thresholds are reached, helping users distinguish between the two reset operations.

## 4. Hardware and System Architecture

The prototype integrates the following components:

| Component | Purpose |
|---|---|
| Arduino MKR WiFi 1010 | Main controller and Wi-Fi connectivity |
| DFRobot Colour Sensor | Colour recognition |
| Capacitive Touch Sensor | User interaction and reset |
| LCD1602 RGB Screen | System status and feedback |
| Adafruit NeoPixel Strip | Local lighting preview |
| ON/OFF Switch | Physical power control |
| Battery | Portable power supply |
| Cardboard Enclosure | Physical structure and component positioning |

The main communication workflow is:

**Colour Sensor → Arduino Processing → Lighting Engine → MQTT Broker → Vespera**

The Lighting Engine generates RGB data for a 12 × 6 LED matrix, producing 216 RGB bytes per complete frame.

The software is organised into separate modules responsible for colour sensing, touch input, LCD feedback, lighting generation and MQTT communication.

This modular approach supports debugging and refinement without requiring the entire program to be rewritten.

## 5. Development Process

ColourPass was developed through five documented stages, progressing from individual component experiments to a physically assembled and network-connected prototype.

| Stage | Development Focus | Documentation |
|---|---|---|
| Part 01 | Initial colour sensor measurements and testing | [Colour Sensor Testing](design-log/part-01-colour-sensing/README.md) |
| Part 02 | Hardware integration, LCD feedback and preliminary colour calibration | [Hardware Integration and Colour Calibration](design-log/part-02-hardware-integration-and-colour-calibration/README.md) |
| Part 03 | Modular software development, MQTT communication and lighting effects | [MQTT Integration and Lighting Control](design-log/part-03-mqtt-integration-and-lighting-control/README.md) |
| Part 04 | Cardboard enclosure construction and sensor positioning improvements | [Physical Prototype Construction](design-log/part-04-physical-prototype-construction/README.md) |
| Part 05 | On-site colour measurements, code refinement and physical Vespera testing | [Colour Recognition and Code Refinement](design-log/part-05-colour-recognition-and-code-refinement/README.md) |

Each development log records the objectives, implementation, testing evidence, observations and limitations of that stage.

## 6. Source Code

The current source files are available in the [code](code/) directory.

## 7. Declaration of Generative AI Use

I acknowledge the use of **ChatGPT (GPT-6, OpenAI, https://chatgpt.com)** as an assistive tool during the development and documentation of ColourPass–Vespera.

AI assistance was used for:

- **Programming support:** Generating, debugging and refining some of the Arduino code, including colour recognition, touch interaction and MQTT communication.
- **Technical guidance:** Explaining hardware integration, communication protocols and troubleshooting approaches.
- **Documentation:** Supporting the organisation, drafting and English-language refinement of GitHub README files.

AI-generated suggestions and code were reviewed, adapted and tested during development. The project concept, physical prototype construction, experimental measurements and final design decisions were my own.

##
*Designed and developed by Monong Yang (Morgan)*

