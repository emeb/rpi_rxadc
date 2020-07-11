/*
 * r820t2.c - R820T2 Tuner driver for Raspberry Pi
 * 07-08-2020 E. Brombaugh
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
#include "r820t2.h"
#include "shared_i2c.h"

/*
 * I2C stuff
 */
#define R820T2_I2C_ADDRESS   0x1A	// 0011010

/*
 * Freq calcs
 */
#define PPM_ERR -48LL
#define XTAL_FREQ 28800000
#define CALIBRATION_LO 88000

/* registers */
#define R820T2_NUM_REGS     0x20
#define R820T2_WRITE_START	5

/* initial values from airspy */
/* initial freq @ 128MHz -> ~5MHz IF due to xtal mismatch */
static const uint8_t r82xx_init_array[R820T2_NUM_REGS] =
{
	0x00, 0x00, 0x00, 0x00,	0x00,		/* 00 to 04 */
    /* 05 */ 0x90, // LNA manual gain mode, init to 0
    /* 06 */ 0x80,
    /* 07 */ 0x60,
    /* 08 */ 0x80, // Image Gain Adjustment
    /* 09 */ 0x40, // Image Phase Adjustment
    /* 0A */ 0xA8, // Channel filter [0..3]: 0 = widest, f = narrowest - Optimal. Don't touch!
    /* 0B */ 0x0F, // High pass filter - Optimal. Don't touch!
    /* 0C */ 0x40, // VGA control by code, init at 0
    /* 0D */ 0x63, // LNA AGC settings: [0..3]: Lower threshold; [4..7]: High threshold
    /* 0E */ 0x75,
    /* 0F */ 0xF8, // Filter Widest, LDO_5V OFF, clk out OFF,
    /* 10 */ 0x7C,
    /* 11 */ 0x83,
    /* 12 */ 0x80,
    /* 13 */ 0x00,
    /* 14 */ 0x0F,
    /* 15 */ 0x00,
    /* 16 */ 0xC0,
    /* 17 */ 0x30,
    /* 18 */ 0x48,
    /* 19 */ 0xCC,
    /* 1A */ 0x60,
    /* 1B */ 0x00,
    /* 1C */ 0x54,
    /* 1D */ 0xAE,
    /* 1E */ 0x0A,
    /* 1F */ 0xC0
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * Tuner frequency ranges
 * Kanged & modified from airspy firmware to include freq for scanning table
 * "Copyright (C) 2013 Mauro Carvalho Chehab"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 */
struct r820t_freq_range
{
  uint16_t freq;
  uint8_t open_d;
  uint8_t rf_mux_ploy;
  uint8_t tf_c;
};

const struct r820t_freq_range freq_ranges[] =
{
  {
  /* 0 MHz */ 0,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0xdf, /* R27[7:0]  band2,band0 */
  }, {
  /* 50 MHz */ 50,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0xbe, /* R27[7:0]  band4,band1  */
  }, {
  /* 55 MHz */ 55,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x8b, /* R27[7:0]  band7,band4 */
  }, {
  /* 60 MHz */ 60,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x7b, /* R27[7:0]  band8,band4 */
  }, {
  /* 65 MHz */ 65,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x69, /* R27[7:0]  band9,band6 */
  }, {
  /* 70 MHz */ 70,
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x58, /* R27[7:0]  band10,band7 */
  }, {
  /* 75 MHz */ 75,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 80 MHz */ 80,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 90 MHz */ 90,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 100 MHz */ 100,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 110 MHz */ 110,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 120 MHz */ 120,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 140 MHz */ 140,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x14, /* R27[7:0]  band14,band11 */
  }, {
  /* 180 MHz */ 180,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 220 MHz */ 220,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 250 MHz */ 250,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x11, /* R27[7:0]  highest,highest */
  }, {
  /* 280 MHz */ 280,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 310 MHz */ 310,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 450 MHz */ 450,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 588 MHz */ 588,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 650 MHz */ 650,
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }
};

/*
 * local vars
 */
uint8_t r820t_regs[R820T2_NUM_REGS];
uint32_t r820t_freq;
uint32_t r820t_xtal_freq;
uint32_t r820t_if_freq;

/*
 * Write single R820T2 reg via I2C
 */
void R820T2_i2c_write_reg(uint8_t reg, uint8_t data)
{
    /* check for legal reg */
    if(reg>=R820T2_NUM_REGS)
        return;
    
    /* update cache */
    r820t_regs[reg] = data;
    
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, R820T2_I2C_ADDRESS) < 0)
		return;
	
    /* send via I2C */
	i2c_smbus_write_byte_data(i2c_file, reg, r820t_regs[reg]);
}

/*
 * Write single R820T2 reg via I2C with mask vs cached
 */
void R820T2_i2c_write_cache_mask(uint8_t reg, uint8_t data, uint8_t mask)
{
    /* check for legal reg */
    if(reg>=R820T2_NUM_REGS)
        return;
    
    /* mask vs cached reg */
    data = (data & mask) | (r820t_regs[reg] & ~mask);
    r820t_regs[reg] = data;
    
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, R820T2_I2C_ADDRESS) < 0)
		return;
	
    /* send via I2C */
	i2c_smbus_write_byte_data(i2c_file, reg, r820t_regs[reg]);
}

/*
 * Nybble-level bit reverse table for register readback
 */
const uint8_t bitrev_lut[16] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };

/*
 * Read R820T2 regs via I2C
 */
uint8_t R820T2_i2c_read_raw(uint8_t *data, uint8_t sz)
{
	uint8_t i, value;
	ssize_t rsz;
	
	/* check for legal reg */
	if(sz>R820T2_NUM_REGS)
		return 1;
    
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, R820T2_I2C_ADDRESS) < 0)
		return 1;
	
	/* do the bus transaction */
	rsz = read(i2c_file, data, sz);
	
	if(rsz == -1)
	{
		printf("R820T2_i2c_read_raw: error %d\n", errno);
		return 1;
	}
	
	/* bit reverse all results */
	for(i=0;i<rsz;i++)
	{
		value = data[i];
        data[i] = (bitrev_lut[value & 0xf] << 4) | bitrev_lut[value >> 4];
	}
	
    /* status */
    return (rsz != sz);
}

/*
 * Read R820T2 reg - uncached
 */
uint8_t R820T2_i2c_read_reg_uncached(uint8_t reg)
{
    uint8_t sz = reg+1;
    uint8_t *data = r820t_regs;
	
    /* check for legal read */
	if(sz>R820T2_NUM_REGS)
		return 0;
    
	/* assign address of R820T2 */
	if(ioctl(i2c_file, I2C_SLAVE, R820T2_I2C_ADDRESS) < 0)
		return 0;
	
    /* get all regs up to & including desired reg */
    if(R820T2_i2c_read_raw(data, sz))
	{
		printf("R820T2_i2c_read_reg_uncached: read error\n");
		return 0;
	}
	
    /* return desired */
    return r820t_regs[reg];
}

/*
 * Read R820T2 reg - cached
 */
uint8_t R820T2_i2c_read_reg_cached(uint8_t reg)
{
    /* check for legal read */
    if(reg>R820T2_NUM_REGS)
        return 0;
    
    /* return desired */
    return r820t_regs[reg];
}

/*
 * r820t tuning logic
 */
#ifdef OPTIM_SET_MUX
int r820t_set_mux_freq_idx = -1; /* Default set to invalid value in order to force set_mux */
#endif

/*
 * Update Tracking Filter
 * Kanged & Modified from airspy firmware
 * 
 * "inspired by Mauro Carvalho Chehab set_mux technique"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 * part of r820t_set_mux() (set tracking filter)
 */
static void R820T2_set_tf(uint32_t freq)
{
    const struct r820t_freq_range *range;
    unsigned int i;

    /* Get Freq in MHz */
    freq = (uint32_t)((uint64_t)freq * 4295 >> 32); // fast approach
    
    /* Scan array for proper range */
    for(i=0;i<ARRAY_SIZE(freq_ranges)-1;i++)
    {
        if (freq < freq_ranges[i + 1].freq)
            break;
    }
    range = &freq_ranges[i];

    /* Open Drain */
    R820T2_i2c_write_cache_mask(0x17, range->open_d, 0x08);

    /* RF_MUX,Polymux */
    R820T2_i2c_write_cache_mask(0x1a, range->rf_mux_ploy, 0xc3);

    /* TF BAND */
    R820T2_i2c_write_reg(0x1b, range->tf_c);

    /* XTAL CAP & Drive */
    R820T2_i2c_write_cache_mask(0x10, 0x08, 0x0b);

    R820T2_i2c_write_cache_mask(0x08, 0x00, 0x3f);
 
    R820T2_i2c_write_cache_mask(0x09, 0x00, 0x3f);
}

/*
 * Update LO PLL
 */
void R820T2_set_pll(uint32_t freq)
{
    const uint32_t vco_min = 1770000000UL;
    const uint32_t vco_max = 3900000000UL;
    uint32_t pll_ref = (r820t_xtal_freq >> 1);
    uint32_t pll_ref_2x = r820t_xtal_freq;

    uint32_t vco_exact;
    uint32_t vco_frac;
    uint32_t con_frac;
    uint32_t div_num;
    uint32_t n_sdm;
    uint16_t sdm;
    uint8_t ni;
    uint8_t si;
    uint8_t nint;

    /* Calculate vco output divider */
    for (div_num = 0; div_num < 5; div_num++)
    {
        vco_exact = freq << (div_num + 1);
        if (vco_exact >= vco_min && vco_exact <= vco_max)
        {
            break;
        }
    }

    /* Calculate the integer PLL feedback divider */
    vco_exact = freq << (div_num + 1);
    nint = (uint8_t) ((vco_exact + (pll_ref >> 16)) / pll_ref_2x);
    vco_frac = vco_exact - pll_ref_2x * nint;

    nint -= 13;
    ni = (nint >> 2);
    si = nint - (ni << 2);

    /* Set the vco output divider */
    R820T2_i2c_write_cache_mask(0x10, (uint8_t) (div_num << 5), 0xe0);

    /* Set the PLL Feedback integer divider */
    R820T2_i2c_write_reg(0x14, (uint8_t) (ni + (si << 6)));

    /* Update Fractional PLL */
    if (vco_frac == 0)
    {
        /* Disable frac pll */
        R820T2_i2c_write_cache_mask(0x12, 0x08, 0x08);
    }
    else
    {
        /* Compute the Sigma-Delta Modulator */
        vco_frac += pll_ref >> 16;
        sdm = 0;
        for(n_sdm = 0; n_sdm < 16; n_sdm++)
        {
            con_frac = pll_ref >> n_sdm;
            if (vco_frac >= con_frac)
            {
                sdm |= (uint16_t) (0x8000 >> n_sdm);
                vco_frac -= con_frac;
                if (vco_frac == 0)
                    break;
            }
        }

        /*
        actual_freq = (((nint << 16) + sdm) * (uint64_t) pll_ref_2x) >> (div_num + 1 + 16);
        delta = freq - actual_freq
        if (actual_freq != freq)
        {
            fprintf(stderr,"Tunning delta: %d Hz", delta);
        }
        */
        
        /* Update Sigma-Delta Modulator */
        R820T2_i2c_write_reg(0x15, (uint8_t)(sdm & 0xff));
        R820T2_i2c_write_reg(0x16, (uint8_t)(sdm >> 8));

        /* Enable frac pll */
        R820T2_i2c_write_cache_mask(0x12, 0x00, 0x08);
    }
}

/*
 * Correct for known PPM error
 */
uint32_t R820T2_correct_freq(uint32_t freq)
{
    int64_t err = freq * PPM_ERR;
    err = err / 1000000;
    
    return freq + err;
}

/*
 * Update Tracking Filter and LO to frequency
 */
void R820T2_set_freq(uint32_t freq)
{
  /* compute desired LO freq */
  uint32_t lo_freq = freq + r820t_if_freq;
  
  /* calibrate using known error */
  lo_freq = R820T2_correct_freq(lo_freq);

  /* apply to hardware */
  R820T2_set_tf(freq);
  R820T2_set_pll(lo_freq);
  r820t_freq = freq;
}

/*
 * Update LNA Gain
 */
void R820T2_set_lna_gain(uint8_t gain_index)
{
  R820T2_i2c_write_cache_mask(0x05, gain_index, 0x0f);
}

/*
 * Update Mixer Gain
 */
void R820T2_set_mixer_gain(uint8_t gain_index)
{
  R820T2_i2c_write_cache_mask(0x07, gain_index, 0x0f);
}

/*
 * Update VGA Gain
 */
void R820T2_set_vga_gain(uint8_t gain_index)
{
  R820T2_i2c_write_cache_mask(0x0c, gain_index, 0x0f);
}

/*
 * Enable/Disable LNA AGC
 */
void R820T2_set_lna_agc(uint8_t value)
{
  value = value != 0 ? 0x00 : 0x10;
  R820T2_i2c_write_cache_mask(0x05, value, 0x10);
}

/*
 * Enable/Disable Mixer AGC
 */
void R820T2_set_mixer_agc(uint8_t value)
{
  value = value != 0 ? 0x10 : 0x00;
  R820T2_i2c_write_cache_mask(0x07, value, 0x10);
}

/*
 * Set IF Bandwidth
 */
void R820T2_set_if_bandwidth(uint8_t bw)
{
    const uint8_t modes[] = { 0xE0, 0x80, 0x60, 0x00 };
    uint8_t a = 0xB0 | (0x0F-(bw & 0x0F));
    uint8_t b = 0x0F | modes[(bw & 0x3) >> 4];
    R820T2_i2c_write_reg(0x0A, a);
    R820T2_i2c_write_reg(0x0B, b);
}

/*
 * Calibrate 
 * Kanged from airspy firmware
 * "inspired by Mauro Carvalho Chehab calibration technique"
 * https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
 * part of r820t_set_tv_standard()
 */
int32_t R820T2_calibrate(void)
{
  int32_t i, cal_code;

  /* try 5x */
  for (i = 0; i < 5; i++)
  {
    /* Set filt_cap */
    R820T2_i2c_write_cache_mask(0x0b, 0x08, 0x60);

    /* set cali clk =on */
    R820T2_i2c_write_cache_mask(0x0f, 0x04, 0x04);

    /* X'tal cap 0pF for PLL */
    R820T2_i2c_write_cache_mask(0x10, 0x00, 0x03);

    /* freq used for calibration */
    R820T2_set_pll(CALIBRATION_LO * 1000);

    /* Start Trigger */
    R820T2_i2c_write_cache_mask(0x0b, 0x10, 0x10);

    usleep(2000);

    /* Stop Trigger */
    R820T2_i2c_write_cache_mask(0x0b, 0x00, 0x10);

    /* set cali clk =off */
    R820T2_i2c_write_cache_mask(0x0f, 0x00, 0x04);

    /* Check if calibration worked */
    cal_code = R820T2_i2c_read_reg_uncached(0x04) & 0x0f;
    if (cal_code && cal_code != 0x0f)
      return 0;
  }

  /* cal failed */
  return -1;
}

/*
 * Initialize the R820T
 */
uint8_t R820T2_init(uint8_t verbose)
{
	uint8_t i=0;
	
    /* test if R820 exists */
    if(R820T2_i2c_read_raw(&i, 1))
    {
        if(verbose)
			fprintf(stderr, "R820T2_init: read_raw failed\n");
        r820t_xtal_freq = 0;
        r820t_if_freq = 0;
        r820t_freq = 0;
        return 1;
    }
    if(i!=0x96)
    {
        if(verbose)
			fprintf(stderr, "R820T2_init: read 0 mismatch\n");
        r820t_xtal_freq = 0;
        r820t_if_freq = 0;
        r820t_freq = 0;
        return 1;
    }
    
    /* initialize some operating parameters */
    r820t_xtal_freq = XTAL_FREQ;
    r820t_if_freq = 5000000;
    r820t_freq = 144000000;
    
    /* initialize the device */
    for(i=R820T2_WRITE_START;i<R820T2_NUM_REGS;i++)
    {
		if(verbose)
			fprintf(stderr, "R820T2_init: writing reg %02X = %02X\n", i,
				r82xx_init_array[i]);
        R820T2_i2c_write_reg(i, r82xx_init_array[i]);
	}

    /* Calibrate */
    if(R820T2_calibrate())
	{
		fprintf(stderr, "R820T2_init: Calibration Failed\n");
	}
	else
	{
		if(verbose)
			fprintf(stderr, "R820T2_init: Calibration Successful\n");
    }
	
    /* set freq after calibrate */
    R820T2_set_freq(r820t_freq);
    
    return 0;
}
