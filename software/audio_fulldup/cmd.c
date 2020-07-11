/*
 * cmd.c - Command parsing routines for STM32F303 breakout SPI to ice5 FPGA
 * 05-11-16 E. Brombaugh
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <curses.h>
#include <math.h>
#include "main.h"
#include "ice_lib.h"
#include "audio.h"
#include "rxadc.h"
#include "r820t2.h"

#define MAX_ARGS 4

/* locals we use here */
char cmd_buffer[256];
char *cmd_wptr;
const char *cmd_commands[] = 
{
	"help",
	"spi_read",
	"spi_write",
	"lo_read",
	"lo_write",
	"tune",
	"set_demod",
	"get_demod",
	"r820t2_read",
	"r820t2_write",
    "r820t2_freq",
    "r820t2_lna_gain",
    "r820t2_mixer_gain",
    "r820t2_vga_gain",
    "r820t2_lna_agc_ena",
    "r820t2_mixer_agc_ena",
    "r820t2_bandwidth",
    "vhf_freq",
	"quit",
	""
};

enum cmd_indices
{
	CMD_HELP,
	CMD_SPI_READ,
	CMD_SPI_WRITE,
	CMD_LO_READ,
	CMD_LO_WRITE,
	CMD_TUNE,
	CMD_SET_DEMOD,
	CMD_GET_DEMOD,
	CMD_R820_READ,
	CMD_R820_WRITE,
    CMD_R820_FREQ,
    CMD_R820_LNA_GAIN,
    CMD_R820_MIXER_GAIN,
    CMD_R820_VGA_GAIN,
    CMD_R820_LNA_AGC_ENA,
    CMD_R820_MIXER_AGC_ENA,
    CMD_R820_BANDWIDTH,
    CMD_VHF_FREQ,
	CMD_QUIT,
	CMD_MAX
};

/* reset buffer & display the prompt */
void cmd_prompt(void)
{
	/* reset input buffer */
	cmd_wptr = &cmd_buffer[0];

	/* prompt user */
	printf("\rCommand>");
}

/* process command line after <cr> */
void cmd_proc(void)
{
	char *token, *argv[MAX_ARGS];
	int argc, cmd, reg;
	unsigned int data;

	/* parse out three tokens: cmd arg arg */
	argc = 0;
	token = strtok(cmd_buffer, " ");
	while(token != NULL && argc < MAX_ARGS)
	{
		argv[argc++] = token;
		token = strtok(NULL, " ");
	}

	/* figure out which command it is */
	if(argc > 0)
	{
		cmd = 0;
		while(*cmd_commands[cmd] != '\0')
		{
			if(strcmp(argv[0], cmd_commands[cmd])==0)
				break;
			cmd++;
		}
	
		/* Can we handle this? */
		if(*cmd_commands[cmd] != '\0')
		{
			printf("\n");

			/* Handle commands */
			switch(cmd)
			{
				case CMD_HELP:
					/* Help */
					printf("help - this message\n");
					printf("spi_read <addr> - FPGA SPI read reg\n");
					printf("spi_write <addr> <data> - FPGA SPI write reg, data\n");
					printf("lo_read - Get LO freq\n");
					printf("lo_write <frequency Hz> - Set LO freq\n");
					printf("tune - enter tuning mode\n");
					printf("set_demod <type> - (0=AM, 1=USB, 2=LSB, 3=U/L, 4=NFM, 5=RAW)\n");
					printf("get_demod - get demod mode\n");
					printf("r820t2_read <addr> - ead reg\n");
					printf("r820t2_write <addr> <data> - write reg, data\n");
					printf("r820t2_freq <frequency> - Set freq in Hz\n");
					printf("r820t2_lna_gain <gain> - Set gain of LNA [0 - 15]\n");
					printf("r820t2_mixer_gain <gain> - Set gain Mixer [0 - 15]\n");
					printf("r820t2_vga_gain <gain> - Set gain VGA [0 - 15]\n");
					printf("r820t2_lna_agc_ena <state> - Enable LNA AGC [0 / 1]\n");
					printf("r820t2_mixer_agc_ena <state> - Enable Mixer AGC [0 / 1]\n");
					printf("r820t2_bandwidth <bw> - Set IF bandwidth [0 - 15]\n");
					printf("vhf_freq <frequency> - Set HF & VHF freq in Hz\n");
					printf("quit - exit program\n");
					break;
	
				case CMD_SPI_READ:
					/* spi_read */
					if(argc < 2)
						printf("spi_read - missing arg(s)\n");
					else
					{
						reg = (int)strtoul(argv[1], NULL, 0) & 0x7f;
						ice_read(bs, reg, &data);
						printf("spi_read: 0x%02X = 0x%08X\n", reg, data);
					}
					break;
	
				case CMD_SPI_WRITE:
					/* spi_write */
					if(argc < 3)
						printf("spi_write - missing arg(s)\n");
					else
					{
						reg = (int)strtoul(argv[1], NULL, 0) & 0x7f;
						data = strtoul(argv[2], NULL, 0);
						ice_write(bs, reg, data);
						printf("spi_write: 0x%02X 0x%08X\n", reg, data);
					}
					break;

				case CMD_LO_READ:
					/* lo_read */
					printf("lo_read: %u Hz\n", rxadc_get_lo());
					break;
	
				case CMD_LO_WRITE:
					/* lo_write */
					if(argc < 2)
						printf("lo_write - missing arg(s)\n");
					else
					{
						data = strtoul(argv[1], NULL, 0);
                        data = rxadc_set_lo(data);
						printf("lo_write: 0x%08X\n", data);
					}
					break;
	
				case CMD_TUNE:
					/* Tuning mode */
					{
						int32_t lo_freq;
						int rxchar;
						char textbuf[80];
						
						/* get current tune value */
						lo_freq = rxadc_get_lo();
						
						/* initialize curses */
						initscr();
						cbreak();
						nodelay(stdscr, TRUE);
						noecho();
						
						clear();
						
						mvaddstr(0, 0, "Tuning Mode - ESC to exit");
						mvaddstr(1, 0, "e - o : increment by 1M - 1");
						mvaddstr(2, 0, "d - l : decrement by 1M - 1");
						mvaddstr(3, 0, "q : next demod");
						mvaddstr(4, 0, "a : next filter");
						mvaddstr(4, 40, "+/- : volume");
						
						while(!exit_program)
						{
							rxchar = getch();
							
							/* check for ESC */
							if(rxchar == 27)
								break;
							
							/* refresh screen */
							sprintf(textbuf, "  LO: %9u Hz  ", lo_freq);
							mvaddstr(8, 0, textbuf);
							sprintf(textbuf, "RSSI: % 4d dB  ", Audio_GetRSSI()-6*rxadc_get_ifgain());
							mvaddstr(9, 0, textbuf);
							sprintf(textbuf, "IF ovl: %d  ", rxadc_get_cicsat());
							mvaddstr(9, 40, textbuf);
							sprintf(textbuf, "Mode: %s  ", audio_demod_names[Audio_GetDemod()]);
							mvaddstr(10, 0, textbuf);
							sprintf(textbuf, "  BW: %d Hz  ", Audio_GetFilterBW(Audio_GetFilter()));
							mvaddstr(11, 0, textbuf);
							sprintf(textbuf, "  IF: %d dB  ", 6*rxadc_get_ifgain());
							mvaddstr(12, 0, textbuf);
							sprintf(textbuf, " Vol: %ld  ", play_vol);
							mvaddstr(12, 40, textbuf);
							if(Audio_GetDemod() == DEMOD_SYNC_AM)
							{
								sprintf(textbuf, "Sync State: %s Sync Frq: %d     ",
									audio_sync_names[Audio_GetSyncSt()], Audio_GetSyncFrq());
								mvaddstr(13, 0, textbuf);
							}
							else
								mvaddstr(13, 0, "                                   ");
							mvaddch(14, 0, ' ');
								
							refresh();

							/* act on keypress */
							if(rxchar != EOF)
							{
								switch(rxchar)
								{
									case 'e': lo_freq +=1000000; break;
									case 'd': lo_freq -=1000000; break;
									case 'r': lo_freq +=100000; break;
									case 'f': lo_freq -=100000; break;
									case 't': lo_freq +=10000; break;
									case 'g': lo_freq -=10000; break;
									case 'y': lo_freq +=1000; break;
									case 'h': lo_freq -=1000; break;
									case 'u': lo_freq +=100; break;
									case 'j': lo_freq -=100; break;
									case 'i': lo_freq +=10; break;
									case 'k': lo_freq -=10; break;
									case 'o': lo_freq +=1; break;
									case 'l': lo_freq -=1; break;
									case 'q': Audio_SetDemod((Audio_GetDemod()+1)%8); break;
									case 'a': Audio_SetFilter((Audio_GetFilter()+1)%audio_num_filts); break;
									case 'z': rxadc_set_ifgain((rxadc_get_ifgain()+1)%8); break;
									case '+': play_vol++; play_vol = play_vol>100 ? 100 : play_vol; mixer_set(play_vol); break;
									case '-': play_vol--; play_vol = play_vol<0 ? 0 : play_vol; mixer_set(play_vol); break;
								}
								lo_freq = lo_freq >= RXADC_FSAMPLE/2 ? RXADC_FSAMPLE/2 : lo_freq;
								lo_freq = lo_freq < 0 ? 0 : lo_freq;
								
								rxadc_set_lo(lo_freq);
							}

							usleep(20000);
						}
						
						/* shut down curses */
						endwin();
					}
					break;
	
				case CMD_SET_DEMOD:
					/* set_demod */
					if(argc < 2)
						printf("set_demod - missing arg\n");
					else
					{
						data = strtoul(argv[1], NULL, 0);
						Audio_SetDemod(data);
						printf("set_demod: %s\n", audio_demod_names[Audio_GetDemod()]);
					}
					break;
	
				case CMD_GET_DEMOD:
					/* get_demod */
					printf("get_demod: %s\n", audio_demod_names[Audio_GetDemod()]);
					break;
	
				case CMD_R820_READ: 	/* r820t2_read */
					if(argc < 2)
						printf("r820t2_read - missing arg(s)\n");
					else
					{
						reg = (int)strtoul(argv[1], NULL, 0) & 0x3f;
						data = R820T2_i2c_read_reg_uncached(reg);
						printf("r820t2_read: 0x%02X = 0x%02X\n", reg, data);
					}
					break;
	
				case CMD_R820_WRITE: 	/* r820t2_write */
					if(argc < 3)
						printf("r820t2_write - missing arg(s)\n");
					else
					{
						reg = (int)strtoul(argv[1], NULL, 0) & 0x3f;
						data = strtoul(argv[2], NULL, 0);
						R820T2_i2c_write_reg(reg, data);
						printf("r820t2_write: 0x%02X 0x%02X\n", reg, data);
					}
					break;
		
                case CMD_R820_FREQ:     /* r820t2_freq */
					if(argc < 2)
						printf("r820t2_freq - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0);
						R820T2_set_freq(data);
						printf("r820t2_freq:  %d\n", data);
					}
                    break;
                    
                case CMD_R820_LNA_GAIN:     /* r820t2_lna gain */
					if(argc < 2)
						printf("r820t2_lna_gain - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0) & 0x0f;
						R820T2_set_lna_gain(data);
						printf("r820t2_lna_gain:  %d\n", data);
					}
                    break;
                
                case CMD_R820_MIXER_GAIN:     /* r820t2_mixer gain */
					if(argc < 2)
						printf("r820t2_mixer_gain - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0) & 0x0f;
						R820T2_set_mixer_gain(data);
						printf("r820t2_mixer_gain:  %d\n", data);
					}
                    break;
                
                case CMD_R820_VGA_GAIN:     /* r820t2_vga gain */
					if(argc < 2)
						printf("r820t2_vga_gain - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0) & 0x0f;
						R820T2_set_vga_gain(data);
						printf("r820t2_vga_gain:  %d\n", data);
					}
                    break;
                
                case CMD_R820_LNA_AGC_ENA:     /* r820t2_lna agc enable */
					if(argc < 2)
						printf("r820t2_lna_agc_ena - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0) & 0x01;
						R820T2_set_lna_agc(data);
						printf("r820t2_lna_agc_ena:  %d\n", data);
					}
                    break;
                
                case CMD_R820_MIXER_AGC_ENA:     /* r820t2_mixer agc enable */
					if(argc < 2)
						printf("r820t2_lna_agc_ena - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0) & 0x01;
						R820T2_set_mixer_agc(data);
						printf("mixer_agc_ena:  %d\n", data);
					}
                    break;
                
                case CMD_R820_BANDWIDTH:     /* r820t2_IF Bandwidth */
					if(argc < 2)
						printf("r820t2_bandwidth - missing arg(s)\n");
					else
					{
						data = (int)strtoul(argv[1], NULL, 0) & 0x0f;
						R820T2_set_if_bandwidth(data);
						printf("r820t2_bandwidth:  %d\n", data);
					}
                    break;

                case CMD_VHF_FREQ:     /* vhf_freq */
					if(argc < 2)
						printf("vhf_freq - missing arg(s)\n");
					else
					{
                        /* set VHF freq */
						data = (int)strtoul(argv[1], NULL, 0);
						printf("vhf_freq:  R820T2 freq = %d\n", data);
						R820T2_set_freq(data);
                        
                        /* set HF Freq to IF */
						{
							rxadc_set_lo(r820t_if_freq);
                            printf("vhf_freq:  DDC IF freq = %d\n", r820t_if_freq);
						}
					}
                    break;

				case CMD_QUIT:
					/* bail out */
					printf("quit:Goodbye\n");
					exit_program = 1;
					break;
	
				

				default:	/* shouldn't get here */
					break;
			}
		}
		else
			printf("Unknown command\n");
	}
}
	
void init_cmd(void)
{
	/* prompt */
	cmd_prompt();
}

void cmd_parse(char ch)
{
	/* accumulate chars until cr, handle backspace */
	if(ch == '\b')
	{
		/* check for buffer underflow */
		if(cmd_wptr - &cmd_buffer[0] > 0)
		{
			printf("\b \b");		/* Erase & backspace */
			cmd_wptr--;		/* remove previous char */
		}
	}
	else if(ch == '\n')
	{
		*cmd_wptr = '\0';	/* null terminate, no inc */
		cmd_proc();
		cmd_prompt();
	}
	else
	{
		/* check for buffer full (leave room for null) */
		if(cmd_wptr - &cmd_buffer[0] < 254)
		{
			*cmd_wptr++ = ch;	/* store to buffer */
			putc(ch, stdout);			/* echo */
		}
	}
	fflush(stdout);
}
