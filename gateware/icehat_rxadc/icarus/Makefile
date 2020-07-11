# Makefile for Icarus Verilog simulation
# 02-11-2019 E. Brombaugh

# sources
SOURCES = 	tb_icehat_rxadc.v ../icestorm/icehat_rxadc.v \
			../src/spi_slave.v \
			../src/ddc_14.v ../src/tuner_2.v ../src/tuner_slice_1k.v \
			../src/cic_dec_4.v ../src/sat_flag.v ../src/sat.v \
			../src/fir8dec_par.v ../src/sine_osc.v ../src/pdm_dac.v \
			../src/wm8731.v ../src/i2c_emul.v ../src/i2s_inout_usb_hi.v

# top level
TOP = tb_icehat_rxadc
			
# Executables
VLOG = iverilog
WAVE = gtkwave
TECH_LIB = /usr/local/share/yosys/ice40/cells_sim.v

# targets
all: $(TOP).vcd

wave: $(TOP).vcd $(TOP).gtkw
	$(WAVE) $(TOP).gtkw
	
$(TOP).vcd: $(TOP)
	./$(TOP)

$(TOP): $(SOURCES) $(HEX)
	$(VLOG) -D icarus -l $(TECH_LIB) -o $(TOP) $(SOURCES)
	
clean:
	rm -rf a.out *.obj $(RPT) $(TOP) $(TOP).vcd
	
