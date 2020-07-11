# Makefile for icestorm tools + yosys + nextpnr
# Modified from examples in icestorm distribution
# 01-16-18 E. Brombaugh

# src directory
VPATH = ../src

SRC =	icehat_rxadc.v ../src/wm8731.v ../src/i2c_emul.v \
		../src/i2s_inout_usb_hi.v ../src/spi_slave.v ../src/pdm_dac.v \
		../src/ddc_14.v ../src/cic_dec_4.v ../src/fir8dec_par.v \
		../src/sat_flag.v ../src/sat.v ../src/tuner_2.v \
		../src/tuner_slice_1k.v ../src/sine_osc.v 
		
# project stuff
PROJ = icehat_rxadc
PIN_DEF = icehat_rxadc.pcf
SDC = icehat_rxadc.sdc
DEVICE = up5k
PACKAGE = sg48

YOSYS = /usr/local/bin/yosys
NEXTPNR = nextpnr-ice40
#NEXTPNR_ARGS = --pre-pack $(SDC) --ignore-loops
NEXTPNR_ARGS = --pre-pack $(SDC)
ICEPACK = icepack
ICETIME = icetime
ICE_TOOL = ice_tool
CDCPROG = cdc_prog
VERILATOR = verilator
TECH_LIB = /usr/local/share/yosys/ice40/cells_sim.v

all: $(PROJ).bin

%.json: $(SRC)
	$(YOSYS) -p 'synth_ice40 -dsp -top $(PROJ) -json $@' $(SRC)

%.asc: %.json $(PIN_DEF) 
	$(NEXTPNR) $(NEXTPNR_ARGS) --$(DEVICE) --json $< --pcf $(PIN_DEF) --package $(PACKAGE) --asc $@

%.bin: %.asc
	$(ICEPACK) $< $@

%.rpt: %.asc
	$(ICETIME) -d $(DEVICE) -mtr $@ $<

#prog: $(PROJ).bin
#	$(ICEPROG) -S $<
    
#prog: $(PROJ).bin
#	$(CDCPROG) -p /dev/ttyACM0 $<

prog: $(PROJ).bin
	$(ICE_TOOL) -v $<
	
sudo-prog: $(PROJ).bin
	@echo 'Executing prog as root!!!'
	sudo $(ICEPROG) -S -IB $<
	
lint: $(SRC)
	$(VERILATOR) --lint-only -Wall --top-module $(PROJ) $(TECH_LIB) $(SRC)

clean:
	rm -f $(PROJ).json $(PROJ).asc $(PROJ).rpt $(PROJ).bin $(HEX)

.SECONDARY:
.PHONY: all prog clean
