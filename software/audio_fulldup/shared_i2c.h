/*
 * shared_i2c.h - common i2c bus interfaces for RPi
 */

#ifndef __shared_i2c__
#define __shared_i2c__

#include <stdint.h>
#include "main.h"

extern int i2c_file;

uint8_t shared_i2c_init(int i2c_bus, uint8_t verbose);
void shared_i2c_free(void);

#endif