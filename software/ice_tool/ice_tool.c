/* ice_tool.c - iCE40 configuration & control for raspberry pi               */
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
#include <unistd.h>
#include "ice_lib.h"

#define VERSION "0.1"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void help(void) __attribute__ ((noreturn));

static void help(void)
{
	fprintf(stderr,
	    "Usage: ice_tool [-r addr][-w addr data][-v][-V] [BITSTREAM] \n"
		"  BITSTREAM is a file containing the FPGA bitstream to download\n"
		"  -r read SPI control port at addr\n"
		"  -w write SPI control port at addr w/ data\n"
		"  -v enables verbose progress messages\n"
		"  -V prints the tool version\n");
	exit(1);
}

int main(int argc, char **argv)
{
	iceblk *bs;
	int flags = 0, read = 0, write = 0, verbose = 0, cfg = 0;
	int addr = 0, data = 0;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-')
	{
		switch (argv[1+flags][1])
		{
		case 'r':
			read = 1;
			if (2+flags < argc)
				addr = atoi(argv[flags+2]);
			flags++;
			break;
		case 'w':
			write = 1;
			if (2+flags < argc)
				addr = atoi(argv[flags+2]);
			if (3+flags < argc)
				data = atoi(argv[flags+3]);
			flags+=2;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			fprintf(stderr, "ice_tool version %s\n", VERSION);
			exit(0);
		default:
			fprintf(stderr, "Error: Unsupported option "
				"\"%s\"!\n", argv[1+flags]);
			help();
			exit(1);
		}
		flags++;
	}
    
    /* configuring? */
	if(argc > flags + 1)
        cfg = 1;

	/* open up hardware */
	if((bs = ice_init(cfg, verbose)) == NULL)
	{
		fprintf(stderr, "Couldn't access hardware\n");
		exit(2);
	}
	
	/* send bitstream to FPGA or SPI Flash if bitfile name available*/
	if(cfg)
	{
        /* Configure FPGA */
        if(verbose == 1) fprintf(stderr, "Configuring FPGA...\n");

        if(ice_cfg(bs, argv[flags + 1]))
        {
            fprintf(stderr, "Error sending bitstream to FPGA\n");
        }
	}

	/* spi read/write? */
	if(read || write)
	{
		int ret;
		uint8_t tx[] = {0x00, 0x00, 0x00, 0x00, 0x00};
		uint8_t rx[ARRAY_SIZE(tx)] = {0, };
		
		/* Build message */
		tx[0] = addr & 0x7f;	// set address
		if(read)
		{
			fprintf(stderr, "Read 0x%02X\n", addr);
			tx[0] |= 0x80;
		}

		if(write)
		{
			fprintf(stderr, "Write 0x%02X 0x%08X\n", addr, data);
			tx[1] = (data >> 24) & 0xff;
			tx[2] = (data >> 16) & 0xff;
			tx[3] = (data >>  8) & 0xff;
			tx[4] = (data >>  0) & 0xff;
		}
 
		/* send a message */
		ret = ice_spi_txrx(bs, tx, rx, ARRAY_SIZE(tx));
		if (ret == 1)
		{
			fprintf(stderr, "Can't send spi message\n");
		}
		else
		{
			/* Dump RX Message */
			fprintf(stderr, "Sent: ");
			for (ret = 0; ret < ARRAY_SIZE(tx); ret++)
			{
				fprintf(stderr, "%.2X ", tx[ret]);
			}
			fprintf(stderr, "\n");
			fprintf(stderr, "Received: ");
			for (ret = 0; ret < ARRAY_SIZE(rx); ret++)
			{
				fprintf(stderr, "%.2X ", rx[ret]);
			}
			fprintf(stderr, "\n");
		}
	}
	
	/* All done */
	ice_delete(bs);
	
	return 0;
}
