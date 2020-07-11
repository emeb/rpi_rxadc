/*
 * Old GPIO access routines kanged from the internet uses sysfs
 */

#ifndef __gpio_sysfs__
#define __gpio_sysfs__

/* GPIO stuff */
#define IN  0
#define OUT 1
 
#define LOW  0
#define HIGH 1

int GPIOExport(int pin);
int GPIOUnexport(int pin);
int GPIODirection(int pin, int dir);
int GPIORead(int pin);
int GPIOWrite(int pin, int value);

#endif
