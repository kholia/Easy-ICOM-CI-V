# https://github.com/arduino/arduino-cli/releases

port := $(shell python3 board_detect.py)

default:
	arduino-cli compile --fqbn=arduino:avr:nano:cpu=atmega328old easy_civ

upload:
	@# echo $(port)
	arduino-cli -v upload -p "${port}" --fqbn=arduino:avr:nano:cpu=atmega328old easy_civ

install_platform:
	arduino-cli core install arduino:avr

deps:
	arduino-cli lib install "Time"
	arduino-cli lib install "RTClib"
	arduino-cli lib install "Streaming"


install_arduino_cli:
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh
