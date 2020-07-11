# rpi_rxadc - Hardware
Hardware for the RPi RXADC presently consists of the following:

## icehat
https://github.com/emeb/icehat

This is a small (RPi Zero sized) FPGA board that comprises a Lattice ICE40
Ultra Plus 5k FPGA and several PMOD connectors. The board natively provides
GPIO connections between the FPGA and the RPi SPI and I2S ports, but I've
haywired additional connections to the RPi UART pins and I2C bus in order to
support emulation of a WM8731 codec which is needed for the DSP interface.

## rxadc_14
https://github.com/emeb/rxadc_14

This is a dual-connector PMOD which provides a 14-bit ADC that can sample at
up to 105MSPS. The input port for the ADC is transformer-coupled and matched
for 50 ohm impedance. For this project I've configured it with a 50MHz
oscillator that is a good match for the sampling rates possible on the icehat
FPGA.

## r820t2
https://github.com/emeb/r820t2

This is a small breakout board for the Raphael R820T2 VHF/UHF front-end tuner
chip. A 4-pin header provides the I2C port required to control the chip, as
well as 5V power (about 200ma). This board extends the tuning range beyond
the 0-20MHz HF band which is possible when the rxadc_14 board is used alone,
providing additional coverage in the 50 - 1500 MHz range.

## Future Plans
The goal is to integrate the three boards listed above, along with an Si5351
clock source and some RF switching / filtering to provide a wide-coverage
SDR receiver in a single Raspberry Pi "Hat" format, sized to fit onto the
larger RPi 3 & 4 boards. Board design will be done with KiCad and the project
files will be kept here.

