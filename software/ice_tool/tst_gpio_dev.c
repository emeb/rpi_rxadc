/*
 * tst_gpio_dev.c - test GPIO char dev lib
 * 06-21-2020 E. Brombaugh
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gpio_dev.h"

enum gpios
{
	DONE_PIN = 23,
	RST_PIN = 24,
	SS_PIN = 25,
};

/*
 * new chardev-based gpio
 */
unsigned int out_lines[] =
{
	RST_PIN,
	SS_PIN
};

unsigned int in_lines[] =
{
	DONE_PIN
};

/* wait for key hit */
void waitkey(void)
{
	printf("Paused - hit a key to continue\n");
	while(fgetc(stdin)==EOF);
}

//#define PAUSE

#ifdef PAUSE
#define wait waitkey()
#else
#define wait usleep(100000)
#endif

int main(int argc, char **argv)
{
	int count = 10;
	
	if(GPIOInit(2, out_lines, 1, in_lines))
	{
		fprintf(stderr, "Couldn't init GPIO\n");
		exit(1);
	}

	while(count--)
	{
		fprintf(stdout, "%d\n", count);
		GPIOWrite(0, 1);
		wait;
		GPIOWrite(0, 0);
		wait;
		GPIOWrite(1, 1);
		wait;
		GPIOWrite(1, 0);
		wait;
	}
	
	GPIOFree();
}