# SMO Development Journal

# SMO 

## 2nd july 2026

### Project Overview

SMO is a handheld embedded systems gadget using Pi Pico 2 W. It  debugging tools, signal analysis, retro games, and hardware utilities
create a pocket sized companion for electronics enthusiasts students

---

# Current Hardware

* Raspberry Pi Pico 2 W
* ST7789 SPI TFT display
* SD card module
* MPU6050 
* 6x buttons
* PWM output
* I2C interface
* SPI peripherals
* ADC support
* USB 2.0

---

# Software Architecture

The firmware is split into multiple Arduino files



## Core

* `SMO.ino`

  * smo.ino (main)
  * Display setup
  * menu setup
  * Button handling
  * Global vars
  * switch cases




---

## Tools

### Oscilloscope

Implemented:

* Analog signal visualization
* Real time waveform
* Grid rendering


---



### GPIO Tool

Current functionality:

* Pin testing

Future improvements:

* Hats
* Live GPIO monitor

---

### I2C Scanner

Implemented:

* Device scanning

---


### System Information

displays hardware information such as:

* Memory
* Clock
* Device information

---

### SD Card Browser

Implemented:

* SD card initialization
* Directory browsing
* File listing


### MPU6050

Integrated for:

* some games
* Gyroscope readings


# games 

## Doom

A lightweight Doom made for smo


# Animations

The firmware contains boot and menu animations that improve ux


# PCB Status


The next milestone is designing a dedicated PCB 

* Pico 2 W
* Display connector
* SD card
* Sensors
* Buttons
* Power management
* gpios for the hats

---

# Code Organization

The project is modular

Each application lives in its own `.ino` file
---

# Current Progress

Completed:

* Menu framework
* Hardware initialization
* Oscilloscope
* I2C scanner
* SD card browser
* System information
* MPU6050 integration
* Retro game support
* Doom integration

---



# Development Philosophy

SMO is designed to be an open-source handheld engineering tool 


*Last Updated: July 2, 2026*
