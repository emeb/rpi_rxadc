/*
 * main.c - test full duplex audio in/out with low-level ALSA lib
 * 07-01-20 E. Brombaugh
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include "main.h"
#include "audio.h"
#include "cmd.h"
#include "rxadc.h"
#include "shared_i2c.h"
#include "r820t2.h"
#include "si5351.h"

/* version */
const char *swVersionStr = "V0.1";

/* build time */
const char *bdate = __DATE__;
const char *btime = __TIME__;

/* constants */
const int legal_rates[3] = {44100, 48000, 88200};

/* state */
iceblk *bs;
int					demod = DEMOD_AM;
int					filter = 0;
char				*snd_device_in = "plughw:0,0";
char 				*snd_device_out = "plughw:0,0";
char				*bitstream_name =
	"/home/pi/rpi/lattice/icehat_rxadc/icestorm/icehat_rxadc.bin";
snd_pcm_t			*playback_handle;
snd_pcm_t			*capture_handle;
snd_mixer_t			*mixer_handle;
snd_mixer_selem_id_t *sid;
snd_mixer_elem_t	*elem;
int                 i2c_bus = 1;
int					nchannels = 2;
int					buffer_size = 4096;
int					sample_rate = 48000;
int 				bits = 16;
int 				err;
int					exit_program = 0;
long				play_vol = 80;
int					vhf = 0;
char				*rdbuf;
unsigned int		fragments = 2;
int					frame_size;
snd_pcm_uframes_t   frames, inframes, outframes;

/*
 * set up an audio device
 */
int configure_alsa_audio(snd_pcm_t *device, int channels)
{
	snd_pcm_hw_params_t *hw_params;
	int                 err;
	unsigned int		tmp;

	/* allocate memory for hardware parameter structure */ 
	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
	{
		fprintf (stderr, "cannot allocate parameter structure (%s)\n",
			snd_strerror(err));
		return 1;
	}
	
	/* fill structure from current audio parameters */
	if((err = snd_pcm_hw_params_any(device, hw_params)) < 0)
	{
		fprintf (stderr, "cannot initialize parameter structure (%s)\n",
			snd_strerror(err));
		return 1;
	}

	/* set access type, sample rate, sample format, channels */
	if((err = snd_pcm_hw_params_set_access(device, hw_params,
		SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		fprintf (stderr, "cannot set access type: %s\n",
			snd_strerror(err));
		return 1;
	}
	
	// bits = 16
	if((err = snd_pcm_hw_params_set_format(device, hw_params,
		SND_PCM_FORMAT_S16_LE)) < 0)
	{
		fprintf (stderr, "cannot set sample format: %s\n",
			snd_strerror(err));
		return 1;
	}
	
	tmp = sample_rate;    
	if((err = snd_pcm_hw_params_set_rate_near(device, hw_params,
		&tmp, 0)) < 0)
	{
		fprintf (stderr, "cannot set sample rate: %s\n",
			snd_strerror(err));
		return 1;
	}
	
	if(tmp != sample_rate)
	{
		fprintf(stderr, "Could not set requested sample rate, %d != %d\n",
			sample_rate, tmp);
		sample_rate = tmp;
	}
	
	if((err = snd_pcm_hw_params_set_channels(device, hw_params, channels)) < 0)
	{
		fprintf (stderr, "cannot set channel count: %s\n",
			snd_strerror(err));
		return 1;
	}
	
	if((err = snd_pcm_hw_params_set_periods_near(device, hw_params,
		&fragments, 0)) < 0)
	{
		fprintf(stderr, "Error setting # fragments to %d: %s\n", fragments,
			snd_strerror(err));
		return 1;
	}

	frame_size = channels * (bits / 8);
	frames = buffer_size / frame_size * fragments;
	if((err = snd_pcm_hw_params_set_buffer_size_near(device, hw_params,
		&frames)) < 0)
	{
		fprintf(stderr, "Error setting buffer_size %lu frames: %s\n", frames,
			snd_strerror(err));
		return 1;
	}
	
	if(buffer_size != frames * frame_size / fragments)
	{
		fprintf(stderr, "Could not set requested buffer size, %d != %lu\n",
			buffer_size, frames * frame_size / fragments);
		buffer_size = frames * frame_size / fragments;
	}

	if((err = snd_pcm_hw_params(device, hw_params)) < 0)
	{
		fprintf(stderr, "Error setting HW params: %s\n",
			snd_strerror(err));
		return 1;
	}
	
	return 0;
}

/*
 * catch ^C
 */
void handle_signals(int s)
{
	printf("Caught signal %d\n",s);
	exit_program = 1;
}

/*
 * audio thread
 */
void *audio_thread_handler(void *ptr)
{
	/* processing loop */
	fprintf(stderr, "Starting Audio Thread\n");
	while(!exit_program)
	{
		/* get input & handle errors */
		while((long)(inframes = snd_pcm_readi(capture_handle, rdbuf, frames)) < 0)
		{
			if(inframes == -EAGAIN)
				continue;
			
			if((err = snd_pcm_recover(capture_handle, (int)inframes, 1)))
				fprintf(stderr, "Input recover failed: %s\n",
					snd_strerror(err));
		}
		
		if(inframes != frames)
			fprintf(stderr, "Short read from capture device: %lu != %lu\n",
				inframes, frames);

		/* now processes the frames */
		Audio_Process(rdbuf, inframes);

		while((long)(outframes = snd_pcm_writei(playback_handle, rdbuf, inframes)) < 0)
		{
			if (outframes == -EAGAIN)
				continue;
			
			if((err = snd_pcm_recover(playback_handle, (int)outframes, 1)))
				fprintf(stderr, "Output recover failed: %s\n",
					snd_strerror(err));
		}
		
		if (outframes != inframes)
			fprintf(stderr, "Short write to playback device: %lu != %lu\n",
				outframes, frames);
	}
	
	fprintf(stderr, "Audio Thread Quitting.\n");
	return NULL;
}

/*
 * initialize the mixer
 */
void mixer_init(void)
{
	const char *card = "default";
	const char *selem_name = "Master";

	snd_mixer_open(&mixer_handle, 0);
	snd_mixer_attach(mixer_handle, card);
	snd_mixer_selem_register(mixer_handle, NULL, NULL);
	snd_mixer_load(mixer_handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);
	elem = snd_mixer_find_selem(mixer_handle, sid);
}

/*
 * set mixer main level
 */
void mixer_set(long volume)
{
	long min, max;

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);	
}

/*
 * top level
 */
int main(int argc, char **argv)
{
	pthread_t audio_thread;
	extern char *optarg;
	int opt;
	struct sigaction sigIntHandler;
	int i, verbose = 0;
	int  iret;
	uint32_t ID;
	int rxchar;
	
	/* parse options */
	while((opt = getopt(argc, argv, "b:d:f:r:vVh")) != EOF)
	{
		switch(opt)
		{
			case 'b':
				/* buffer size */
				buffer_size = atoi(optarg);
				break;

			case 'd':
				/* demod */
				{
					/* convert option arg to uppercase */
					char *c = optarg;
					while(*c)
					{
						*c = toupper(*c);
						c++;
					}
				}
				
				/* search in demods for arg */
				for(demod=0;demod<DEMOD_MAX;demod++)
					if(strcmp(optarg, audio_demod_names[demod])==0)
						break;
				if(demod == DEMOD_MAX)
				{
					fprintf(stderr, "Unknown demod type: %s\n", optarg);
					exit(1);
				}
				break;
			
			case 'f':
				/* filter bw */
				filter = atoi(optarg);
			
				/* search for nearest BW */
				for(i=0;i<audio_num_filts;i++)
					if(filter>=Audio_GetFilterBW(i))
						break;
				filter = i<5 ? i : 4;
				break;
					
			case 'r':
				/* sample rate */
				sample_rate = atoi(optarg);
			
				/* search for rate */
				for(i=0;i<3;i++)
					if(sample_rate==legal_rates[i])
						break;
				if(i==3)
				{
					fprintf(stderr, "Illegal sample rate: %s\n", optarg);
					exit(1);
				}
				break;
				
			case 'v':
				verbose = 1;
				break;
			
			case 'V':
				fprintf(stderr, "%s version %s\n", argv[0], swVersionStr);
				exit(0);
			
			case 'h':
			case '?':
				fprintf(stderr, "USAGE: %s [options]\n", argv[0]);
				fprintf(stderr, "Version %s, %s %s\n", swVersionStr, bdate, btime);
				fprintf(stderr, "Options: -b <Buffer Size>    Default: %d\n", buffer_size);
				fprintf(stderr, "         -d <demod>          Default: %s\n", audio_demod_names[demod]);
				fprintf(stderr, "         -f <filter BW kHz>  Default: %d\n", Audio_GetFilterBW(filter));
				fprintf(stderr, "         -r <sample rate Hz> Default: %d\n", sample_rate);
				fprintf(stderr, "         -v enables verbose progress messages\n");
				fprintf(stderr, "         -V prints the tool version\n");
				fprintf(stderr, "         -h prints this help\n");
				exit(1);
		}
	}
	
	/* open up hardware */
	if((bs = ice_init(1, verbose)) == NULL)
	{
		fprintf(stderr, "Couldn't access hardware\n");
		exit(2);
	}
	
	/* check if FPGA needs to be configured */
	if(ice_read(bs, RXADC_REG_ID, &ID))
	{
		fprintf(stderr, "Error reading ID\n");
		ice_delete(bs);
		exit(1);
	}		
	
	if((ID & 0xFFFFFF00) != 0xADC50000)
	{
        /* Configure FPGA */
        if(verbose == 1)
		{
			fprintf(stderr, "ID mismatch - Expected 0x%06X, got 0x%06X\n", 0xADC500, ID>>8);
			fprintf(stderr, "Configuring FPGA\n");
		}
		
        if(ice_cfg(bs, bitstream_name))
        {
            fprintf(stderr, "Error sending bitstream to FPGA\n");
			ice_delete(bs);
			exit(1);
        }
		
		/* Update ID */
		if(ice_read(bs, RXADC_REG_ID, &ID))
		{
			fprintf(stderr, "Error reading ID\n");
			ice_delete(bs);
			exit(1);
		}
	}
	
	/* set DAC mux */
	rxadc_set_dacmux(RXADC_ENABLE);
	
	/* open up I2C bus */
	if(shared_i2c_init(i2c_bus, verbose))
	{
		if(verbose)
			fprintf(stderr, "I2C bus init failed\n");
		ice_delete(bs);
		exit(1);
	}
	
	/* check for R820 front end */
	if(R820T2_init(verbose))
	{
		if(verbose)
			fprintf(stderr, "R820T2 not found - using HF mode\n");
		vhf = 0;
	}
	else
	{
		if(verbose)
			fprintf(stderr, "R820T2 found - using VHF mode\n");
		vhf = 1;
	}
	
	/* check for Si5351 clock generator */
	if(si5351_init(verbose))
	{
		if(verbose)
			fprintf(stderr, "Si5351 not found - ignoring\n");
	}
	else
	{
		si5351_set_output_chl(0, 50000000);
		si5351_set_output_chl(1, 25000000);
		if(verbose)
			fprintf(stderr, "Si5351 configured for 0:50MHz, 1:25MHz\n");
	}
	
	/* set up for control c */
	sigIntHandler.sa_handler = handle_signals;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	/* open input and output devices */
	if((err = snd_pcm_open(&capture_handle, snd_device_in, SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		fprintf(stderr, "cannot open input audio device %s: %s\n", snd_device_out, snd_strerror(err));
		ice_delete(bs);
		exit(1);
	}

	if((err = snd_pcm_open(&playback_handle, snd_device_out, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
	{
		fprintf(stderr, "cannot open input audio device %s: %s\n", snd_device_in, snd_strerror(err));
		snd_pcm_close(capture_handle);
		ice_delete(bs);
		exit(1);
	}
	
	/* set up both devices identically */
	configure_alsa_audio(capture_handle,  nchannels);
	configure_alsa_audio(playback_handle, nchannels);
	
	/* init the mixer */
	mixer_init();
	mixer_set(play_vol);

	/* set up sizes */
	frame_size = nchannels * (bits / 8);
	fprintf(stderr, "Bytes/Frame = %d\n", frame_size);
	frames = buffer_size / frame_size;
	fprintf(stderr, "Frames/buffer = %lu\n", frames);
	
	/* allocate the audio buffer */
	rdbuf = (char *)malloc(buffer_size);
		
	/* prepare for use */
	snd_pcm_prepare(capture_handle);
	snd_pcm_prepare(playback_handle);

	/* fill the whole output buffer */
	for(i = 0; i < fragments; i += 1)
		snd_pcm_writei(playback_handle, rdbuf, frames);
	
	/* set up audio processing */
	Audio_Init();
	Audio_SetDemod(demod);
	Audio_SetFilter(filter);
	fprintf(stderr, "Demod: %s, Filter: %d Hz\n", audio_demod_names[demod], Audio_GetFilterBW(filter));
	
	/* start audio thread */
	fprintf(stderr, "main: starting audio thread...\n");
	iret = pthread_create(&audio_thread, NULL, audio_thread_handler, NULL);
	if(!iret)
	{	
		/* wait for ^C */
		fprintf(stderr, "Starting Command Process Loop\n");
		init_cmd();
		while(!exit_program)
		{
			/* wait a bit */
			usleep(20000);
			
#if 0
			/* status */
			fprintf(stderr, "RSSI: %d dBm  ", Audio_GetRSSI());
			if(demod == DEMOD_SYNC_AM)
				fprintf(stderr, "Sync State: %s Sync Frq: %d     ",
					audio_sync_names[Audio_GetSyncSt()], Audio_GetSyncFrq());
			fprintf(stderr, "\r");
#else
			/* cmd handler */
			if((rxchar = getchar())!= EOF)
			{
				/* Parse commands */
				cmd_parse(rxchar);
			}
#endif
		}
		fprintf(stderr, "main: finishing...\n");
		
		pthread_join(audio_thread, NULL);
		fprintf(stderr, "main: audio thread joined...\n");
	}
	else
		fprintf(stderr, "main: error creating audio thread\n");
	
	/* clean up */
	snd_pcm_drain(playback_handle);
	snd_pcm_drop(capture_handle);
	free(rdbuf);
	snd_mixer_close(mixer_handle);
	snd_pcm_close(playback_handle);
	snd_pcm_close(capture_handle);
	shared_i2c_free();
	ice_delete(bs);

	return 0;
}
