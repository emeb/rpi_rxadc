/*
 * audio.h - audio processing
 * 07-01-20 E. Brombaugh
 */

#ifndef __audio__
#define __audio__

enum demods
{
	DEMOD_AM,
	DEMOD_SYNC_AM,
	DEMOD_USB,
	DEMOD_LSB,
	DEMOD_ULSB,
	DEMOD_NBFM,
	DEMOD_RAW,
	DEMOD_MAX
};

enum syncstate
{
	SYNC_UNLOCK,
	SYNC_WIDE,
	SYNC_NARROW
};

extern const char *audio_demod_names[DEMOD_MAX];
extern const char *audio_sync_names[];
extern const uint8_t audio_num_filts;

void Audio_Init(void);
void Audio_SetFilter(uint8_t filter);
uint8_t Audio_GetFilter(void);
uint32_t Audio_GetFilterBW(uint8_t filter);
int16_t Audio_GetRSSI(void);
void Audio_SetDemod(uint8_t demod);
int8_t Audio_GetDemod(void);
void Audio_SetMute(uint8_t State);
uint16_t Audio_GetMute(void);
int16_t Audio_GetParam(void);
int16_t Audio_GetSyncFrq(void);
int16_t Audio_GetSyncSt(void);
void Audio_Process(char *rdbuf, int inframes);

#endif
