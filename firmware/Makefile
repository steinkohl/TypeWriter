# Build and flash the Mega firmware with arduino-cli (runs fine on the Pi 5).
#
#   arduino-cli core install arduino:avr     (once)
#   make            compile
#   make upload     flash (stop typewriterd first: it holds the serial port)

FQBN   ?= arduino:avr:mega
PORT   ?= /dev/ttyACM0
SKETCH := TypewriterBridge

.PHONY: all upload clean

all:
	arduino-cli compile --fqbn $(FQBN) --build-path build $(SKETCH)

upload: all
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) --input-dir build $(SKETCH)

clean:
	rm -rf build
