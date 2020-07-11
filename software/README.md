# rpi_rxadc - Software
Software design for the RPi RXADC consists of several different tools

* ice_tool - a command line application that can load FPGA bitstreams
and also perform simple read / write operations to the embedded SPI
interface in the gateware.

* audio_fulldup - a demonstration user interface that manages the SDR
operation and does realtime demodulation of the received I & Q data stream.
It supports a simple command interpreter for setting up and testing the
receiver state and also provides an ncurses-based realtime tuning control
panel that provides feedback on various settings and conditions.



