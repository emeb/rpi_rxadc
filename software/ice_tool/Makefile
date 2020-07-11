#OBJS = ice_tool.o ice_lib.o gpio_sysfs.o
OBJS = ice_tool.o ice_lib.o gpio_dev.o
CFLAGS = -Wall

all: $(OBJS)
	$(CC) $^ -o ice_tool 
	
tst_gpio_dev: tst_gpio_dev.o gpio_dev.o
	$(CC) $^ -o tst_gpio_dev
	
clean:
	rm -f *.o *~ core ice_tool
