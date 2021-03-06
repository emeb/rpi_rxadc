/*
 * audio_lib.h - miscellaneous audio processing functions
 */

#ifndef __audio_lib__
#define __audio_lib__

int16_t audio_sat(int32_t in);
void audio_split_stereo(int16_t sz, int16_t *src, int16_t *ldst, int16_t *rdst);
uint8_t audio_clip(int16_t sz, int16_t *src, int16_t thresh);
void audio_sum_stereo(int16_t sz, int16_t *lsrc, int16_t *rsrc);
void audio_copy(int16_t sz, int16_t *dst, int16_t *src);
void audio_sop3(int16_t sz, int16_t *dst, int16_t *src1, int16_t *src2, float32_t g0,
	float32_t g1, float32_t g2);
void audio_sop2(int16_t sz, int16_t *dst, int16_t *src1, float32_t g0, float32_t g1);
void audio_gain(int16_t sz, int16_t *dst, int16_t *src, float32_t gain);
void audio_gain_sum(int16_t sz, int16_t *dst, int16_t *src, float32_t gain);
void audio_comb_stereo(int16_t sz, int16_t *dst, int16_t *lsrc, int16_t *rsrc);
void audio_morph(int16_t sz, int16_t *dst, int16_t *asrc, int16_t *bsrc,
				float32_t morph);
//void audio_cpmix(int16_t sz, int16_t *dst, int16_t *asrc, int16_t *bsrc,
//				int16_t mix);
void audio_cp2mix(int16_t sz, int16_t *dst, int16_t *asrc, int16_t *bsrc,
				float32_t mix);

#endif
