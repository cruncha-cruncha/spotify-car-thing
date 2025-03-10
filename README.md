# spotify-car-thing
When I'm learning the lyrics of a new song, I want to listen to small sections over and over and over, not start from the beginning every time. This project is an attempt to solve this very serious and intractable problem. Along the way, we'll learn about the Spotify Web API, iPhone Shortcuts, and the ~~ESP32~~ Raspi Pico. This project does not use a Spotify Car Thing, but I use it with Spotify in my car.

This project started off using the "Adafruit ESP32-S3 Reverse TFT Feather - 4MB Flash, 2MB PSRAM, STEMMA QT", but moved to the Raspberry Pi Pico for reasons I won't go into here.

### Hardware
- Raspberry Pi Pico W
- breadboard
- 4 tactile switches
- a rotary encoder (quadrature encoding, 24 indents, with momentary switch)

The four buttons are labelled "red", "yellow", "green", and "blue". 
- red: restart board
- yellow: reconnect to wifi
- green: like the current song
- blue: go back ~15 seconds

### Software
You're looking at it. Be advised: I am not a C++, Ardunio, nor embedded developer. This code is some horrible abomination of Object Oriented and Procedural. It uses the Arduino-Pico library to compile.

Button presses are detected by core0 (aka the frontend) and handled by core1 (aka the backend); each button corresponds to an integer, and on press that integer is pushed to a fifo shared by the cores. One loop of core1 might run like below:
1. Fifo is emptied, all values increment a corresponding variable.
2. Variable blue_click_buffer > 0, which means the blue button was pressed
3. Fetch the current song and it's playback position
4. Empty the fifo again, and increment corresponding variables
5. Skip back 16 * blue_click_buffer seconds
6. Handle other button presses (without fetching the current song again)
7. Reset click_buffer variables back to zero
Step 3 takes ~1 second, in which time the user (me) might have clicked the blue button again (because I want to skip back ~30 seconds, not ~15). The code will detect a second button press after fetching the current song, and skip back 32 seconds (which ends up being ~31 seconds from the time I initially clicked the blue button).

## Setup
Please see secrets.example.h

## Notes
Currently the rotary encoder doesn't do anything, do to an unknown difficulty with the Spotify Web API: it refuses to change volume. The red and yellow buttons are desirable as the wifi connection is very shaky. I've set up a shortcut automation on my phone to automatically turn on hotspot when it connects to my car's bluetooth, and the pico connects to that.
