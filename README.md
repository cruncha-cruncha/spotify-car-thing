# spotify-car-thing
When I'm learning the lyrics of a new song, I want to listen to small sections over and over and over, not start from the beginning every time. This project is an attempt to solve this very serious and intractable problem. Along the way, we'll learn about the Spotify Web API, iPhone Shortcuts, and the ESP32. This project does not use a Spotify Car Thing, but I use it with Spotify in my car.

### Hardware
- "Adafruit ESP32-S3 Reverse TFT Feather - 4MB Flash, 2MB PSRAM, STEMMA QT", [link](https://www.adafruit.com/product/5691). This board is great becuase it has: a screen, buttons, wifi connectivity, a low power mode, and a lipo battery charger. 
- "Lithium Ion Polymer Battery - 3.7v 750mAh", [link](https://www.pishop.ca/product/lithium-ion-polymer-battery-3-7v-450mah/). I would have bought 1000mAh but it was out of stock.

### Software
You're looking at it. Be advised: I'm not a C++ developer.

## How?
The board has three buttons. I've decided to use them as follows:
1. Sleep / wake
2. Like / Un-Like a song
3. Go back 15 seconds

## Setup
Please see secrets.example.h

## Notes
I am not a C++, Ardunio, nor embedded developer. This code is some horrible abomination of Object Oriented and Procedural.
