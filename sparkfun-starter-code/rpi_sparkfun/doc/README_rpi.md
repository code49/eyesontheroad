# README Raspberry Pi

To view release notes use the following link:
https://developer.acconeer.com/sw-release-notes/

## 1 Setup Raspberry Pi 2, 3 or 4

Follow the instructions on https://www.raspberrypi.org/downloads/ to install Raspberry Pi OS.
Use of the 32-bit version of the OS is recommended, instructions for how to use the 64-bit
version of the OS can be found below.

### 1.1 After starting up the Raspberry Pi:

Start a terminal window and type "sudo raspi-config"
- In Localisation Options, select the appropriate timezone.
- In Interfacing Options, enable SPI and I2C and the SSH interfaces.

Install libgpio2:
sudo apt install libgpiod2

If you plan to use XC112 with newer kernels (v5.4 and later) the following line must
be added to /boot/config.txt (i.e. sudo nano /boot/config.txt) and then reboot.

dtoverlay=spi0-1cs,cs0_pin=8

The reason for this is that the kernel spi driver controls and prevent any user application
from controlling the SPI0 CS1 which is by default mapped to [GPIO7](https://pinout.xyz/pinout/pin26_gpio7).
The above line will disable the usage of SPI0 CS1 and release the GPIO7 so that it can be controlled by
the Acconeer SW.

### 1.2 Using Acconeer 32-bit binaries on 64-bit system (arm64)

Add 32-bit architecture:
sudo dpkg --add-architecture armhf
sudo apt update

Install 32-bit libraries:
sudo apt install libc6:armhf libgpiod2:armhf

## 2 Development environment

The software can be built either on a standalone Linux system or directly on the
Raspberry Pi. Both methods should work equally well.

### 2.1 Setup for development on Raspberry Pi

Make sure that the following packages are installed: gcc, make.
Use "apt-get install [package]" if needed.

### 2.2 Setup for development on standalone Linux system

The instructions are verified for Debian-based Linux distributions (such as Ubuntu).

Make sure that the following packages are installed: gcc-arm-linux-gnueabihf, make
Use "apt-get install [package]" if needed.

## 3 Distributed files

Extract the zip-file you got from Acconeer and look at the file structure.

- makefile and rule/ contain all makefiles to build the example programs.
- lib/*.a are pre-built Acconeer software.
- include/*.h are interface descriptions used by applications.
- source/example_*.c are applications to use the Acconeer API to communicate with the sensor.
- source/acc_board_*.c are board support files to handle target hardware differences.
  for reference.
- doc/ contains HTML documentation for all source files. Open doc/rss_api.html .
- out/ contains pre-built applications (same as executing "make" again).

## 4 Building the software

- Enter the directory that you extracted in section 3.
- To build the example programs, type "make" (the ZIP file already contains pre-built versions of them).
- All files created during build are stored in the out/ directory.
- "make clean" will delete the out/ directory.

## 5 Executing the software

First you need to transfer the executable to the Raspberry Pi (unless the zip-file was already extracted to the Raspberry Pi.).

Then start the application using:

- ./out/example_detector_distance
