/*
 * rxadc.h - access routines for rxadc FPGA functions
 * 07-06-20 E. Brombaugh
 */

#ifndef __rxadc__
#define __rxadc__

#include "main.h"

#define RXADC_FSAMPLE 50000000
#define RXADC_LOBITS 26

enum rxadc_regs
{
	RXADC_REG_ID,
	RXADC_REG_LO = 0x10,
	RXADC_REG_DACMUX,
	RXADC_REG_NSENA,
	RXADC_REG_CICSHF,
	RXADC_REG_CICSAT = 0x15
};

enum rxadc_states
{
	RXADC_DISABLE,
	RXADC_ENABLE
};

uint32_t rxadc_get_lo(void);
uint32_t rxadc_set_lo(uint32_t freqHz);
void rxadc_set_dacmux(uint8_t state);
uint8_t rxadc_get_ifgain(void);
void rxadc_set_ifgain(uint8_t cic_shift);
uint8_t rxadc_get_cicsat(void);

#endif
