# ESP32 Water Meter

A mini ESP32 water jug meter project using a display, pushbutton, and future HX711/load cell support.

Working Demo: https://wokwi.com/projects/468491779475203073


## Current Status

- ESP32 display demo is working
- Button input is wired on GPIO 13
- Button uses INPUT_PULLUP
- Button wiring: GPIO 13 → button → GND
- Future goal: show water jug percentage, refill animation, and send readings to a hub

## Hardware

- ESP32 development board
- Small display
- Pushbutton
- Breadboard
- Jumper wires
- Future: HX711 load cell amplifier
- Future: load cell / weight sensor

## Button Wiring

The button is wired like this:

GPIO 13 → button → GND

The code uses internal pull-up logic:

- Not pressed = HIGH
- Pressed = LOW

## Planned Features

- Water percentage display
- Refill animation
- Splash screen
- Wi-Fi on/off by button press
- Hub display for water meter and plant moisture sensors
- Daily water use estimate
- Days until empty estimate
