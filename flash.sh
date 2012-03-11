#!/bin/sh
make clean && make && avrdude -c avrisp2 -p m328p -P usb -U flash:w:soundboard.hex
