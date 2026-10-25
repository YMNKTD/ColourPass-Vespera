
# Part 01 — Colour Sensor Testing


**Status:** Initial sensor reading completed

## 1. Objective

Test whether the DFRobot colour sensor can provide distinguishable readings for different colours using an Arduino MKR WiFi 1010.

The Vespera installation was not available during home testing. For later development, lighting commands will be simulated using the Arduino Serial Monitor.

## 2. Wiring

| Sensor pin | Arduino pin |
| --- | --- |
| VDD | VCC (3.3V) |
| GND | GND |
| OE | GND |
| S0 | 0 |
| S1 | 1 |
| S2 | 2 |
| S3 | 3 |
| OUT | 4 |


## 3. Testing Method

The Arduino program reads the red, green, and blue sensor channels and prints pulse widths in microseconds to the Serial Monitor at 115200 baud.

Initial experiments used coloured physical objects. To test sensor response using a controlled digital colour source, the experiment was continued with a smartphone displaying red, green, blue, and white.

Screen brightness, distance, angle and ambient lighting can influence the readings. These conditions should be recorded more precisely in future tests.

## 4. Results

The following values are representative observations from screenshots, not averages.

| Screen colour | Hex | R (µs) | G (µs) | B (µs) |
| --- | --- | ---: | ---: | ---: |
| Red | #FF0000 | 179 | 2090 | 1554 |
| Green | #00FF00 | 446 | 282 | 880 |
| Blue | #0000FF | 3637 | 1016 | 284 |
| White | #FFFFFF | 108 | 164 | 180 |


## 5. Observations

- The red screen produced the shortest pulse width on the R channel.
- The green screen produced the shortest pulse width on the G channel.
- The blue screen produced the shortest pulse width on the B channel.
- The white screen produced relatively short pulse widths on all three channels.
- Pulse widths are not standard 0–255 RGB values. Shorter pulse widths generally indicate higher sensor output frequencies.

These initial observations demonstrate distinguishable sensor responses under the test conditions. They do not establish reliable recognition of physical colour cards.

  
