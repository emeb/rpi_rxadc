# Makefile for audio_fulldup - testing audio in/out

# sub directories
VPATH = .:../ice_tool

TOP = audio_fulldup

OBJS = 	main.o audio.o iir.o audio_lib.o ice_lib.o gpio_dev.o cmd.o \
		rxadc.o shared_i2c.o r820t2.o si5351.o

CFLAGS = -Wall -O3 -I ../ice_tool

all: $(TOP)

$(TOP): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ -lasound -lm -lpthread -lcurses -li2c

# enable core dumps with
# ulimit -c unlimited
	
debug: $(TOP)
	gdb -q -nh $(TOP) core

clean:
	rm -f *.o *~ core $(TOP)
