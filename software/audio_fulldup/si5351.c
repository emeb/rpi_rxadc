/*
 * si5351.c - SI5351 Clock Generator driver for STM32F030
 * 01-08-2017 E. Brombaugh
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <math.h>
#include <string.h>
#include "shared_i2c.h"
#include "si5351.h"

/*
 * I2C stuff
 */
#define SI5351_I2C_ADDRESS   0x60

/*
 * Freq calcs
 */
#define XTAL_FREQ 25000000
#define C_MAX ((1<<20)-1)

/*
 * local vars
 */
uint32_t si5351_vco_freq[2];
uint8_t si5351_databuffer[8];

/*
 * send a single reg / byte to Si5351
 */
uint8_t si5351_i2c_write_reg(uint8_t reg, uint8_t data)
{
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, SI5351_I2C_ADDRESS) < 0)
		return 1;
	
    /* send via I2C */
	i2c_smbus_write_byte_data(i2c_file, reg, data);
	
	return 0;
}

/*
 * Send a block of data to the Si5351 via I2C
 */
uint8_t si5351_i2c_write_8(uint8_t reg, uint8_t *data)
{
	uint8_t buffer[9];
	
	/* set up output buffer */
	buffer[0] = reg;
	memcpy(&buffer[1], data, 8);
	
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, SI5351_I2C_ADDRESS) < 0)
		return 1;
	
    /* send via I2C */
	if(write(i2c_file, buffer, 9) != 9)
		return 1;
	
	return 0;
}

/*
 * Get a single reg / byte from Si5351
 */
uint8_t si5351_i2c_read_reg(uint8_t reg, uint8_t *data)
{
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, SI5351_I2C_ADDRESS) < 0)
		return 1;
	
    /* send via I2C */
	*data = i2c_smbus_read_byte_data(i2c_file, reg);
	
	return 0;
}

/*
 * compute values for multisynths in PLLs and outputs
 */
void si5351_multisynth_set(uint8_t *buffer, uint8_t a, uint32_t b, uint32_t c)
{
	uint32_t P1, P2, P3;
	
	/* Set the main Px config registers */
	if(!b)
	{
		/* Integer mode */
		P1 = 128 * a - 512;
		P2 = b;
		P3 = c;
	}
	else
	{
		/* Fractional mode */
#if 0
        /* floating pt */
		P1 = (uint32_t)(128 * a + floor(128 * ((float32_t)b/(float32_t)c)) - 512);
		P2 = (uint32_t)(128 * a - c * floor(128 * ((float32_t)b/(float32_t)c)));
#else
        /* fixed point */
        uint64_t idiv;
        idiv = ((uint64_t)a<<7) + ((((uint64_t)b<<20)/C_MAX)>>(20-7))-512;
        P1 = idiv;
        idiv = ((uint64_t)b<<7) - C_MAX * ((((uint64_t)b<<20)/C_MAX)>>(20-7));
        P2 = idiv;
#endif        
		P3 = c;
	}

	/* break the regs down as the hardware expects */
	buffer[0] = (P3 & 0x0000FF00) >> 8;
	buffer[1] = P3 & 0x000000FF;
	buffer[2] = (P1 & 0x00030000) >> 16;
	buffer[3] = (P1 & 0x0000FF00) >> 8;
	buffer[4] = P1 & 0x000000FF;
	buffer[5] = ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16);
	buffer[6] = (P2 & 0x0000FF00) >> 8;
	buffer[7] = P2 & 0x000000FF;
}

/*
 * set integer divide bits for output multisynths only
 */
void si5351_oms_div_bits(uint8_t *buffer, uint8_t odiv, uint8_t div4)
{
	buffer[2] |= ((odiv & 0x7) << 4);
	
	if(div4)
		buffer[2] |= 0xC;
}

/*
 * Initialize the SSD1306
 */
uint8_t si5351_init(uint8_t verbose)
{
	uint8_t i;

	/* Disable all outputs */
	if(si5351_i2c_write_reg(SI5351_3_Output_Enable_Control, 0xFF))
	{
		if(verbose)
			fprintf(stderr, "si5351_init: OE write failed\n");
		return 1;
	}
	
	/* Power down output drivers */
	for(i=0;i<8;i++)
		si5351_databuffer[i] = 0x80;
	if(si5351_i2c_write_8(SI5351_16_CLK0_Control, si5351_databuffer))
	{
		if(verbose)
			fprintf(stderr, "si5351_init: CLK0 Control write failed\n");
		return 1;
	}

	/* Set XTAL load default 10pf */
	if(si5351_i2c_write_reg(SI5351_183_Crystal_Internal_Load_Capacitance, 0xD2))
	{
		if(verbose)
			fprintf(stderr, "si5351_init: Load Cap write failed\n");
		return 1;
	}


    /* init pll vco values */
    si5351_vco_freq[0] = 0;
    si5351_vco_freq[1] = 0;
	
	if(verbose)
		fprintf(stderr, "si5351_init: successful\n");
	
	return 0;
}

/*
 * set output channel freq - this assumes:
 * - plla -> chl 0, pllb -> chl 1
 * - integer output division for best jitter
 */
uint8_t si5351_set_output_chl(uint8_t chl, uint32_t freq)
{
    int8_t ms_div, a, reg;
    uint32_t Ftarget, b;
    
	/* only good for 2 chls */
	if(chl > 1)
		return 1;
	
#if 0
    /* floating pt math too big to fit into F030 */
	/* get output integer div ratio */
    float32_t div;
    
    ms_div = 2 * floor((900.0e6/(float32_t)freq)/2.0F);
    
    /* Desired VCO freq */
    Ftarget = freq * ms_div;
    
    /* floating pt ratio */
    div = (float32_t)Ftarget / XTAL_FREQ;
    
    /* integer part */
    a = floor(div);
    b = floor((div - (float32_t)a) * (float32_t)C_MAX + 0.5F);
    
    /* update pll vco freq */
    si5351_vco_freq[chl] = floor((float32_t)XTAL_FREQ * ((float32_t)a + (float32_t)b/(float32_t)C_MAX) + 0.5F); 
#else
    /* fixed point math needs uint64_t */
    uint64_t idiv;
    
    ms_div = 0xFE&(900000000/freq);
    
    /* Desired VCO freq */
    Ftarget = freq * ms_div;
    
    /* fixed point ratio */
    idiv = ((uint64_t)Ftarget<<21) / XTAL_FREQ;
    a = idiv >> 21;
    b = ((idiv & ((1<<21)-1)) + 1)>>1;

    /* update pll vco freq */
    idiv = (XTAL_FREQ*(((uint64_t)b<<20)/C_MAX+((uint64_t)a<<20))+(1<<19))>>20;
    si5351_vco_freq[chl] = idiv; 
#endif

    /* compute pll multisync & send */
    si5351_multisynth_set(si5351_databuffer, a, b, C_MAX);
    reg = chl ? SI5351_34_Multisynth_NB_Parameters_0 : SI5351_26_Multisynth_NA_Parameters_0;
    si5351_i2c_write_8(reg, si5351_databuffer);
    
    /* compute output multisync & send */
    si5351_multisynth_set(si5351_databuffer, ms_div, 0, 1);
    si5351_oms_div_bits(si5351_databuffer, 0, ms_div==4);
    reg = chl ? SI5351_50_Multisynth1_Parameters_0 : SI5351_42_Multisynth0_Parameters_0;
    si5351_i2c_write_8(reg, si5351_databuffer);
	
	/* reset the PLL */
	if(chl)
	{
		/* PLL B */
		si5351_databuffer[0] = 0x80;
	}
	else
	{
		/* PLL A */
		si5351_databuffer[0] = 0x20;
	}
	si5351_i2c_write_reg(SI5351_177_PLL_Reset, si5351_databuffer[0]);
   
    /* set output channel parameters */
    if(chl)
    {
        /* powerup,integer,PLLB,uninverted,MS1,8ma */
        si5351_databuffer[0] = 0x6F;
        reg = SI5351_17_CLK1_Control;
    }
    else
    {
        /* powerup,integer,PLLA,uninverted,MS0,8ma */
        si5351_databuffer[0] = 0x4F;
        reg = SI5351_16_CLK0_Control;
    }
    si5351_i2c_write_reg(reg, si5351_databuffer[0]);
    
    /* enable output channel */
    si5351_i2c_read_reg(SI5351_3_Output_Enable_Control, si5351_databuffer);
    if(chl)
    {
        /* enable Chl 1 */
        si5351_databuffer[0] &= 0x02;
    }
    else
    {
        /* enable Chl 0 */
        si5351_databuffer[0] = 0x01;
    }
    si5351_i2c_write_reg(SI5351_3_Output_Enable_Control, si5351_databuffer[0]);
	
	return 0;
}
