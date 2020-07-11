/*
 * main.h - stuff that's needed 
 * 07-01-20 E. Brombaugh
 */

#ifndef __main__
#define __main__

#include <stdint.h>
#include <math.h>
#include "ice_lib.h"

typedef float float32_t;
#define PI 3.14159265358979F

extern int sample_rate;
extern int exit_program;
extern iceblk *bs;
extern long play_vol;
void mixer_set(long vol);

#endif