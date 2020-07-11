# rpi_rxadc - Gateware
The FPGA design (Gateware) for the RPi RXADC is coded in Verilog and targets
the Lattice ICE40UP5k-SG48 chip which provides about 5k logic cells, 8 DSP
cores, 120kbits block RAM and 1024kbits single-port RAM. While somewhat small
compared to FPGAs typically used for SDR applications, this device has plenty
of resources for a simple SDR downconverter that performs well.

## Toolchain
One of the primary enablers for this project is the excellent Open-Source
FPGA toolchain provided by Symbiotic EDA and their Icestorm project:

*https://www.symbioticeda.com/
*https://github.com/YosysHQ
*http://www.clifford.at/icestorm/

This suite of tools, consisting of synthesis, place & route and the chip
databases for the Lattice ICE40 family of parts enables a comprehensive FPGA
design flow that supports excellent performance with high-level DSP constructs
required for this SDR application. Amazingly, it's even possible to run the
entire design, synthesis and test process directly on the Raspberry Pi.

## Architecture
A block diagram of the FPGA architecture is shown below:

![FGPA Architecture](https://github.com/emeb/rpi_rxadc/blob/master/documents/fpga_0.png)
