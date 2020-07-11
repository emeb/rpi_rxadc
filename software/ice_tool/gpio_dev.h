/*
 * New GPIO access routines kanged from the internet uses /dev/gpio
 */

#ifndef __gpio_dev__
#define __gpio_dev__

#include <linux/gpio.h>

int gpiotools_request_linehandle(const char *device_name, unsigned int *lines,
				 unsigned int nlines, unsigned int flag,
				 struct gpiohandle_data *data,
				 const char *consumer_label);
int gpiotools_set_values(const int fd, struct gpiohandle_data *data);
int gpiotools_get_values(const int fd, struct gpiohandle_data *data);
int gpiotools_release_linehandle(const int fd);
int GPIOInit(int nout, unsigned int out_lines[], int nin, unsigned int in_lines[]);
void GPIOFree(void);
int GPIORead(int pin);
int GPIOWrite(int pin, int value);

#endif