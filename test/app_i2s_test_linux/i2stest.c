/*
 * Xylon logiI2S IP core ALSA driver test application
 * Designed for use with ZED board.
 *
 * Copyright (C) 2015 Xylon d.o.o.
 * Author: Davor Joja <davor.joja@logicbricks.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "adau1761.h"


#define I2C_CONTROLLER "/dev/i2c-0"
#define CODEC_I2C_ADDRESS     ((0x38+3)<<1)
#define CODEC_INPUT_FREQUENCY 12000000
const codec_mode mode= CODEC_MODE_MASTER;

#define SOUND_CARD_PLAY   "hw:0,1"
#define SOUND_CARD_RECORD "hw:0,0"
#define SOUND_CARD_RECORD_FORMAT "-fS16_LE"
#define SOUND_CARD_RECORD_CHANNELS "-c2"

#define FILE_NAME_SIZE          (63+1)
#define FILE_FORMAT_SIZE        (3+1)
#define FILE_FORMAT_UNSUPPORTED  0

#define DUMMY_PCM_FILE "/tmp/dummy.pcm"


struct wav_header
{
	char file_type[4];
	unsigned int file_size;
	char file_header_type[4];
	char format_chunk_marker[4];
	unsigned int format_data_length;
	unsigned short format_type;
	unsigned short channels;
	unsigned long sample_rate;
	unsigned int bytes_per_sec;
	unsigned short block_align;
	unsigned short bits_per_sample;
	char data_chunk_header[4];
	unsigned int data_chunk_size;
};

struct config
{
	int sample_rate;
	int rec_time;
	char file_name[FILE_NAME_SIZE];
	char format_type[FILE_FORMAT_SIZE];
	bool rec;
};

static struct config cfg;
static char cmd[128];

static int codec_init(void);

void app_exit(int sig)
{
	printf("Exit!\n");
	exit(1);
}

static void print_usage(char *prg)
{
	fprintf(stderr,
        "Supported WAV format: PCM, 2 channel, 16 bits per sample\n"
        "Codec ADAU1761 operate in %s mode\n"
        "Usage: %s [options]\n"
        "\n"
        "Options: -h        (this help)\n"
        "         -t type   (record file format type [raw, wav])\n"
        "         -r rate   (audio sample rate in HZ; ignored only with WAV format in play mode)\n"
        "         -d sec    (record time duration in seconds; selects record mode)\n"
        "\n"
        "Examples:\n"
        "    %s sound.pcm -traw -r48000         (Play PCM file)\n"
        "    %s sound.pcm -traw -r48000 -d10    (Record PCM)\n"
        "    %s sound.wav -twav                 (Play WAV file)\n"
        "    %s sound.wav -twav -r48000 -d10    (Record WAV)\n",
        (mode == CODEC_MODE_MASTER)? "MASTER":"SLAVE", prg, prg, prg, prg, prg);

        exit(1);
}

static int create_tmp_file(char *file_name, int len)
{
	int fd, wr;
	char tmp=0;

	fd = open(file_name, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
	if (fd == -1)
	{
		printf("%s\n", file_name );
		perror("error creating file");
		return -1;
	}
	while (len--)
	{
		wr = write(fd, &tmp, 1);
		if (wr <= 0)
		{
			printf("%s\n", file_name );
			perror("error writing file");
			close(fd);
			return -1;
		}
	}
	close(fd);
	return 0;
}

static int check_wav(struct config *cfg)
{
	struct wav_header wav_hdr;
	int fd, rd;

	fd = open(cfg->file_name, O_RDONLY);
	if (fd == -1)
	{
		printf("%s\n", cfg->file_name);
		perror("error opening file");
		return -1;
	}

	rd = read(fd, &wav_hdr, sizeof(struct wav_header));
	if (rd <= 0)
	{
		perror("error reading wav header");
		close(fd);
		return -1;
	}
	close(fd);
	/*
	printf("%s\n%d\n%s\n%s\n%d\n%hd\n%hd\n%d\n%d\n%hd\n%hd\n%s\n%d\n",
			wav_hdr.file_type, wav_hdr.file_size, wav_hdr.file_header_type,
			wav_hdr.format_chunk_marker, wav_hdr.format_data_length, wav_hdr.format_type,
			wav_hdr.channels, wav_hdr.sample_rate, wav_hdr.bytes_per_sec,
			wav_hdr.block_align, wav_hdr.bits_per_sample,
			wav_hdr.data_chunk_header, wav_hdr.data_chunk_size);
	*/
	if (strncmp(wav_hdr.file_type, "RIFF", 4))
		return -1;
	if (strncmp(wav_hdr.file_header_type, "WAVE", 4))
		return -1;
	if (strncmp(wav_hdr.format_chunk_marker, "fmt", 3))
		return -1;
	if (wav_hdr.format_type != 1)
	{
		puts("unsupported WAV format type");
		return -1;
	}
	if (wav_hdr.channels != 2)
	{
		puts("unsupported WAV channels number");
		return -1;
	}
	if (wav_hdr.bits_per_sample != 16)
	{
		puts("unsupported WAV bits per sample");
		return -1;
	}

	cfg->sample_rate = wav_hdr.sample_rate;

	return 0;
}

static int codec_check_sample_rate(unsigned long sample_rate)
{
	return adau1761_check_sample_rate(sample_rate);
}

static int codec_init(void)
{
	return adau1761_init(I2C_CONTROLLER, CODEC_I2C_ADDRESS,CODEC_INPUT_FREQUENCY,mode,cfg.sample_rate);
}

static int alsa_check_sample_rate(unsigned long sample_rate)
{
	const unsigned long sampling_rates[] = {
							8000, 11025, 16000, 22050, 32000, 44100, 48000, \
							64000, 88200, 96000, 176000, 192000
						};
	int i, size;

	size = sizeof(sampling_rates) / sizeof(sampling_rates[0]);

	for (i = 0; i < size; i++)
		if (sample_rate == sampling_rates[i])
			return 0;

	return -1;
}

static int check_cfg(struct config *cfg)
{
	if (strlen(cfg->format_type) == FILE_FORMAT_UNSUPPORTED)
	{
		puts("Unsupported file format with -t option.");
		return -1;
	}

	if (cfg->rec || (strcmp(cfg->format_type, "raw") == 0) || (check_wav(cfg) == 0))
	{
		if (codec_check_sample_rate(cfg->sample_rate) == -1)
		{
			printf("Wrong codec sample rate %d\n", cfg->sample_rate);
			return 1;
		}

		if (alsa_check_sample_rate(cfg->sample_rate) == -1)
		{
			printf("Wrong alsa sample rate %d\n", cfg->sample_rate);
			return 1;
		}
	}
	else
		return 1;

	return 0;
}

int main(int argc, char *argv[])
{
	int opt, ret;

	signal(SIGINT, app_exit);

	while ((opt = getopt(argc, argv, "r:d:t:h")) != -1)
	{
		switch (opt)
		{
		case 'r':
			cfg.sample_rate = atoi(optarg);
			break;

		case 'd':
			cfg.rec_time = atoi(optarg);
			cfg.rec = true;
			break;

		case 't':
			if ((strcmp(optarg, "raw") == 0) || (strcmp(optarg, "wav") == 0))
				strcpy(cfg.format_type, optarg);
			else
				cfg.format_type[0] = FILE_FORMAT_UNSUPPORTED;
			break;

		case 'h':
		default:
			print_usage(argv[0]);
			break;
		}
	}
	if (argv[optind] == NULL)
	{
		print_usage(argv[0]);
		exit(1);
	}
	strcpy(cfg.file_name, (argv[optind]));

	ret = check_cfg(&cfg);
	switch (ret)
	{
	case -1:
		print_usage(argv[0]);
		exit(1);
	case 1:
		exit(1);
	}

	if (codec_init() < 0)
		exit(1);

	if (cfg.rec)	/* recording */
	{
		if (mode == CODEC_MODE_SLAVE)
		{
			if (create_tmp_file(DUMMY_PCM_FILE,32) <0)
				return -1;
			// init i2s TX instance (playback) due to it being used by i2s RX instance (capture)
			sprintf(cmd, "aplay --device=%s %s %s -r%d -traw %s", SOUND_CARD_PLAY, SOUND_CARD_RECORD_FORMAT, SOUND_CARD_RECORD_CHANNELS, cfg.sample_rate, DUMMY_PCM_FILE);
			printf("%s\n", cmd);
			system(cmd);
		}
		sprintf(cmd, "arecord --device=%s %s %s -r%d -t%s -d%d %s", SOUND_CARD_RECORD, SOUND_CARD_RECORD_FORMAT, SOUND_CARD_RECORD_CHANNELS, cfg.sample_rate, cfg.format_type, cfg.rec_time, cfg.file_name);
	}
	else	/* playing */
	{
		if (strcmp(cfg.format_type, "wav") == 0)
			sprintf(cmd, "aplay --device=%s -t%s %s", SOUND_CARD_PLAY, cfg.format_type, cfg.file_name);
		else
			sprintf(cmd, "aplay --device=%s %s %s -r%d -t%s %s", SOUND_CARD_PLAY, SOUND_CARD_RECORD_FORMAT, SOUND_CARD_RECORD_CHANNELS, cfg.sample_rate, cfg.format_type, cfg.file_name);
	}
	printf("%s\n", cmd);
	system(cmd);

	return 0;
}

