/*
 * shared_i2c.c - common I2C bus routines
 * 07-10-20 E. Brombaugh
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "shared_i2c.h"

int i2c_file;

uint8_t shared_i2c_init(int i2c_bus, uint8_t verbose)
{
	char filename[20];

	/* open I2C bus */
	sprintf(filename, "/dev/i2c-%d", i2c_bus);
	i2c_file = open(filename, O_RDWR);

	if(i2c_file < 0)
	{
		if(verbose)
			fprintf(stderr, "shared_i2c_init: Couldn't open I2C device %s\n", filename);
		return 1;
	}
	else if(verbose)
		fprintf(stderr, "shared_i2c_init: opened I2C device %s\n", filename);
	
	return 0;
}

/*
 * close i2c device
 */
void shared_i2c_free(void)
{
	close(i2c_file);
}

