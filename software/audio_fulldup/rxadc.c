/*
 * rxadc.c - access routines for rxadc FPGA functions
 * 07-06-20 E. Brombaugh
 */

#include <stdio.h>
#include "rxadc.h"
#include "main.h"

/*
 * get hardware LO frequency
 */
uint32_t rxadc_get_lo(void)
{
	uint32_t freqHz;
	float32_t frq;
	
	/* get raw LO frequency scaled to sample rate */
	ice_read(bs, RXADC_REG_LO, &freqHz);
	
	/* convert to Hz */
	frq = (float32_t)RXADC_FSAMPLE * (float32_t)freqHz/((float32_t)(1<<RXADC_LOBITS));
	freqHz = floorf(frq + 0.5F);
	
	return freqHz;
}

/*
 * set hardware LO frequency
 */
uint32_t rxadc_set_lo(uint32_t freqHz)
{
	/* scale to sample rate */
	float32_t frq = ((float32_t)(1<<RXADC_LOBITS)) * (float32_t)freqHz/(float32_t)RXADC_FSAMPLE;
	freqHz = floorf(frq + 0.5F);
	ice_write(bs, RXADC_REG_LO, freqHz);
	
	/* return actual frequency */
	return rxadc_get_lo();
}

/*
 * set DAC mux
 */
void rxadc_set_dacmux(uint8_t state)
{
	ice_write(bs, RXADC_REG_DACMUX, state);
}

/*
 * get IF gain
 */
uint8_t rxadc_get_ifgain(void)
{
	uint32_t cic_shift;
	
	ice_read(bs, RXADC_REG_CICSHF, &cic_shift);
	
	return cic_shift;
}

/*
 * set IF gain
 */
void rxadc_set_ifgain(uint8_t cic_shift)
{
	ice_write(bs, RXADC_REG_CICSHF, cic_shift);
}

/*
 * get CIC saturation count
 */
uint8_t rxadc_get_cicsat(void)
{
	uint32_t cic_sat;
	
	ice_read(bs, RXADC_REG_CICSAT, &cic_sat);
	
	return cic_sat;
}

