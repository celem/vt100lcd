TARGET       = vt100lcd
# BOARD_TAG: do make showboards to list all valid boards
# WARNING: Modify vt100lcd.h to match cpu type
# make clean
# make
# make ispload

BOARD_TAG    = attiny84-20

ARDUINO_DIR  = /home/ecomer/arduino/arduino-1.0
ARDUINO_MAKEFILE_HOME = /home/ecomer/arduino
ARDUINO_LIBS = NewLiquidCrystal SoftwareSerial
ARDUINO_PORT = /dev/ttyUSB*

# .SECONDARY: will cause the intermediary files to be kept
# .SECONDARY:

include $(ARDUINO_MAKEFILE_HOME)/Arduino.mk

