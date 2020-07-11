/* ice_lib.c - iCE40 interface lib for raspberry pi                          */
/* Copyright 2016 Eric Brombaugh                                             */
/*   This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <errno.h>
#include <math.h>
#include "ice_lib.h"
#include "gpio_dev.h"

#define READBUFSIZE 4096
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* SPI stuff */
#define SPI_BUS 0
#define SPI_ADD 0

enum gpio_pins
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
enum out_idx
{
	RST_IDX,
	SS_IDX
};

unsigned int in_lines[] =
{
	DONE_PIN
};
enum in_idx
{
	DONE_IDX
};

/*
 * My stuff based on BCC history
 */

/* wrapper for fprintf(stderr, ...) to support verbose control */
void qprintf(iceblk *s, char *fmt, ...)
{
	va_list args;
	
	if(s->verbose)
	{
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
	}
}

/* SPI Transmit/Receive */
int ice_spi_txrx(iceblk *s, uint8_t *tx, uint8_t *rx, __u32 len)
{
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.delay_usecs = 0,
		.speed_hz = 15600000,
		.bits_per_word = 8,
	};
	
	return ioctl(s->spi_file, SPI_IOC_MESSAGE(1), &tr);
}

/* wait for key hit */
void waitkey(void)
{
	printf("Paused - hit a key to continue\n");
	while(fgetc(stdin)==EOF);
}


/* initialize our FPGA interface */
iceblk *ice_init(int cfg, int verbose)
{
	iceblk *s;
	char filename[20];
	uint8_t mode = 0;
	//uint8_t mode = SPI_CPHA;
	//uint8_t mode = SPI_CPOL;
	
	/* allocate the object */
	if((s = calloc(1, sizeof(iceblk))) == NULL)
	{
		qprintf(s, "ice_init: Couldn't allocate bfpga object\n");
		goto fail0;
	}
	
	/* set verbose level */
	s->verbose = verbose;
    
    /* do we need to configure? */
    s->cfg = cfg;
    if(cfg)
    {
		qprintf(s, "ice_init: opening GPIO\n");

		if(GPIOInit(2, out_lines, 1, in_lines))
		{
			qprintf(s, "ice_init: Couldn't init GPIO\n");
			goto fail1;
		}
		
		/* Check if FPGA already configured */
		if(GPIORead(DONE_IDX)==0)
			qprintf(s, "ice_init: FPGA not already configured - DONE not high\n\r");
		else
			qprintf(s, "ice_init: FPGA already configured - Done is high\n");

	}
    
	/* Open the SPI port */
	sprintf(filename, "/dev/spidev%d.%d", SPI_BUS, SPI_ADD);
	s->spi_file = open(filename, O_RDWR);
	
	if(s->spi_file < 0)
	{
		qprintf(s, "ice_init: Couldn't open spi device %s\n", filename);
		goto fail2;
	}
	else
		qprintf(s, "ice_init: opened spi device %s\n", filename);

	if(ioctl(s->spi_file, SPI_IOC_WR_MODE, &mode) == -1)
	{
		qprintf(s, "ice_init: Couldn't set SPI mode\n");
		goto fail2;
	}
	else
		qprintf(s, "ice_init: Set SPI mode\n");
	
	/* success */
	return s;

	/* failure modes */
//fail3:
//	close(s->spi_file);		/* close the SPI device */
fail2:
	if(s->cfg)
		GPIOFree();
fail1:
	free(s);				/* free the structure */
fail0:
	return NULL;
}

/* Send a bitstream to the FPGA */
int ice_cfg(iceblk *s, char *bitfile)
{
	FILE *fd;
    int read;
	long ct;
	unsigned char dummybuf[READBUFSIZE];
	char readbuf[READBUFSIZE];

	/* open file or return error*/
	if(!(fd = fopen(bitfile, "r")))
	{
		qprintf(s, "ice_cfg: open file %s failed\n\r", bitfile);
		return 0;
	}
	else
	{
		qprintf(s, "ice_cfg: opened bitstream file %s\n\r", bitfile);
	}

    /* set SS low for spi config */
	GPIOWrite(SS_IDX, 0);
    
	/* pulse RST low min 500 ns */
	GPIOWrite(RST_IDX, 0);
	usleep(1);			// wait a bit
	
	/* Wait for DONE low with timeout */
	qprintf(s, "ice_cfg: RST low, Waiting for DONE low\n\r");
	ct = 0;
	while((GPIORead(DONE_IDX)!=0) && (ct < 1000))
	{
		ct++;
	}
	if(ct>=1000)
		qprintf(s, "ice_cfg: Timeout Waiting for DONE low\n\r");

	/* Release RST */
	GPIOWrite(RST_IDX, 1);

	/* wait 1200us */
	usleep(1200);
	qprintf(s, "ice_cfg: Sending bitstream\n\r");
	
	/* Read file & send bitstream to FPGA via SPI */
	ct = 0;
	while( (read=fread(readbuf, sizeof(char), READBUFSIZE, fd)) > 0 )
	{
		/* Send bitstream */
		ice_spi_txrx(s, (unsigned char *)readbuf, dummybuf, read);
		ct += read;
		
		/* diagnostic to track buffers */
		qprintf(s, ".");
		if(s->verbose)
			fflush(stdout);
	}
	
	/* close file */
	qprintf(s, "\n\rice_cfg: sent %ld bytes\n\r", ct);
	qprintf(s, "ice_cfg: bitstream sent, closing file\n\r");
	fclose(fd);

 	qprintf(s, "ice_cfg: sending dummy clocks\n");
	memset(readbuf, 0, 10);
	ice_spi_txrx(s, (unsigned char *)readbuf, dummybuf, 10);
	
    /* set SS high */
	GPIOWrite(SS_IDX, 1);
    
	/* return status */
	if(GPIORead(DONE_IDX)==0)
	{
		qprintf(s, "ice_cfg: cfg failed - DONE not high\n\r");
		return 1;	// Done = 0 - error
	}
	else	
	{
		qprintf(s, "ice_cfg: success\n\r");
		return 0;	// Done = 1 - OK
	}
}

/*
 * read a SPI slave register
 */
uint8_t ice_read(iceblk *s, uint8_t reg, uint32_t *data)
{
	uint8_t tx[] = {0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	uint8_t ret;
	
	/* Build read header */
	tx[0] = 0x80 | (reg & 0x7f);	// set address

	/* send a message */
	ret = ice_spi_txrx(s, tx, rx, ARRAY_SIZE(tx));
	
	/* assemble result */
	*data = (rx[1]<<24) | (rx[2]<<16) | (rx[3]<<8) | rx[4];
	
	return ret == -1;
}

/*
 * write a SPI slave register
 */
uint8_t ice_write(iceblk *s, uint8_t reg, uint32_t data)
{
	uint8_t tx[] = {0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	
	/* Build write header */
	tx[0] = reg & 0x7f;	// set address
	tx[1] = (data >> 24) & 0xff;
	tx[2] = (data >> 16) & 0xff;
	tx[3] = (data >>  8) & 0xff;
	tx[4] = (data >>  0) & 0xff;

	/* send a message */
	return ice_spi_txrx(s, tx, rx, ARRAY_SIZE(tx)) == -1;
}

/* Clean shutdown of our FPGA interface */
void ice_delete(iceblk *s)
{
	/* Release SPI bus */
	close(s->spi_file);		/* close the SPI device */
	if(s->cfg)
		GPIOFree();
    free(s);				/* free the structure */
}
