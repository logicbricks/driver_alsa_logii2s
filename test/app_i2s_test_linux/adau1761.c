/*
 * ADAU1761 codec initialization functions
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
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include "i2c.h"
#include "adau1761.h"

static int i2c_fd;
static unsigned short int codec_i2c_address;

struct audio_codec_configuration
{
	uint16_t address;
	uint8_t data;
};

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/*
 * swap - swap value of @a and @b
 */
#define swap(a, b) \
        do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)


/* Greatest common divisor */
unsigned long gcd(unsigned long a, unsigned long b)
{
	unsigned long r;

	if (a < b)
		swap(a, b);
	if (!b)
		return a;
	while ((r = a % b) != 0)
	{
		a = b;
		b = r;
	}

	return b;
}

static int calc_pll_values(unsigned int freq_in, unsigned int freq_out,uint8_t pll_regs[6] )
{
	unsigned int r, n, m, i, j;
	unsigned int div;

	if (freq_in < 8000000 || freq_in > 27000000)
		return -EINVAL;

	if (!freq_out)
	{
		r = 0;
		n = 0;
		m = 0;
		div = 0;
	}
	else
	{
		if (freq_out % freq_in != 0)
		{
			div = DIV_ROUND_UP(freq_in, 13500000);
			freq_in /= div;
			r = freq_out / freq_in;
			i = freq_out % freq_in;
			j = gcd(i, freq_in);
			n = i / j;
			m = freq_in / j;
			div--;
		}
		else
		{
			r = freq_out / freq_in;
			n = 0;
			m = 0;
			div = 0;
		}
		if (n > 0xffff || m > 0xffff || div > 3 || r > 8 || r < 2)
			return -EINVAL;
	}

	pll_regs[0] = m >> 8;
	pll_regs[1] = m & 0xff;
	pll_regs[2] = n >> 8;
	pll_regs[3] = n & 0xff;
	pll_regs[4] = (r << 3) | (div << 1);
	if (m != 0)
		pll_regs[4] |= 1; /* Fractional mode */
	pll_regs[5] = 1;

	return 0;
}

static int adau1761_write(uint16_t address, uint8_t value)
{
	i2c_msg_t msg;
	uint8_t buf[3];
	int ret;

	buf[0] = address >> 8;
	buf[1] = (uint8_t)address;
	buf[2] = value;

	i2c_msg_set(&msg, codec_i2c_address, FALSE, buf, sizeof(buf));
	ret = i2c_msg_send(i2c_fd, &msg, 1);
	if (ret < 0)
	{
		printf("Error writing codec at 0x%02X: register 0x%04x\n", codec_i2c_address, address);
		perror("");
	}
	usleep(2000);

	return ret;
}

static int adau1761_set_sample_rate(int base_sampling_rate, int sampling_rate)
{
	uint8_t div, dsp_div;
	int ret;

	if ((base_sampling_rate * 1024) % sampling_rate != 0)
		return -EINVAL;

	switch ((base_sampling_rate * 1024) / sampling_rate)
	{
	case 1024: /* fs */
		div = 0;
		dsp_div = 1;
		break;
	case 6144: /* fs / 6 */
		div = 1;
		dsp_div = 6;
		break;
	case 4096: /* fs / 4 */
		div = 2;
		dsp_div = 5;
		break;
	case 3072: /* fs / 3 */
		div = 3;
		dsp_div = 4;
		break;
	case 2048: /* fs / 2 */
		div = 4;
		dsp_div = 3;
		break;
	case 1536: /* fs / 1.5 */
		div = 5;
		dsp_div = 2;
		break;
	case 512: /* fs / 0.5 */
		div = 6;
		dsp_div = 0;
		break;
	default:
		return -EINVAL;
	}

	ret = adau1761_write(0x4017, div);
	if(ret < 0) return ret;

	ret = adau1761_write(0x40f8, div);
	if(ret < 0) return ret;

	ret = adau1761_write(0x40eb, dsp_div);
	if(ret < 0) return ret;

	return ret;
}

static int adau1761_pll_regs_write(uint8_t *pllData)
{
	i2c_msg_t msg;
	uint8_t buf[2+6];
	int i, ret;

	buf[0] = 0x4002 >> 8;
	buf[1] = (uint8_t)0x4002;
	for(i = 0; i < 6; i++)
		buf[2 + i] = *pllData++;

	i2c_msg_set(&msg, codec_i2c_address, FALSE, buf, sizeof(buf));
	ret = i2c_msg_send(i2c_fd, &msg, 1);
	if (ret < 0)
	{
		printf("Error writing codec at 0x%02X: register 0x%04x\n", codec_i2c_address, 0x4002);
		perror("");
	}
	usleep(2000);

	return ret;
}

int adau1761_check_sample_rate(unsigned long sample_rate)
{
	const unsigned long adau1761_sampling_rates[] = {	8000, 12000, 16000, 24000, 32000, 48000, 96000, \
														7350, 11025, 14700, 22050, 29400, 44100, 88200 };
	int i, size;

	size = sizeof(adau1761_sampling_rates) / sizeof(adau1761_sampling_rates[0]);

	for (i = 0; i < size; i++)
		if (sample_rate == adau1761_sampling_rates[i])
			return 0;

	return -1;
}

int adau1761_init(char *i2c_controller, unsigned short int i2c_addr, int freq_in, codec_mode mode, int sampling_rate)
{
	uint8_t pll_regs[6];
	int i, ret, base_sampling_rate;

	const struct audio_codec_configuration adau1761_config[] =
	{
			//Setting all the input mixers
			{0x400a, 0x01},		//Record mixer left control 0;	Enable the mixer
			{0x400b, 0x05},		//Record mixer left control 1;	MX1AUXG = 0 db
			{0x400c, 0x01},		//Record mixer right control 0;	Just enable the mixer
			{0x400d, 0x05},		//Record mixer right control 1; MX2AUXG = 0 db
			//Setting all the output mixers
			{0x401c, 0x21},		//Playback mixer left 0; left DAC input unmuted
			{0x401e, 0x41},		//Playback Mixer right 0; right DAC input unmuted

			{0x4020, 0x03},		//Playback mixer right L/R
			{0x4021, 0x09},		//Playback mixer left L/R

			{0x4023, 0xe7},		//Left Headphone volume =0 db, +unmute +enable
			{0x4024, 0xe7},		//Right Headphone volume =0 db, +unmute +headphone mode

			{0x4025, 0xe6},		//Playback Line out volume left
			{0x4026, 0xe6},		//Playback Line out volume right

			{0x4019, 0x03},		//Set up the ADC; ADCs Enabled: Both on

			{0x4029, 0x03},		//Set up the DAC; Playback left + right channel enabled.
			{0x402a, 0x03},		//DACs Enabled: Both on

			{0x40f2, 0x01},		//Serial Input Route Control: Serial input [L0, R0] to DACs [L, R]
			{0x40f3, 0x01},		//Serial Output Route Control: ADCs [L, R] to serial output [L0, R0]

			{0x40f9, 0x7f},		//Power up the various parts of the cores; Power up all the modules
			{0x40fa, 0x03}		//Enable the two clocks
	};

	ret = adau1761_check_sample_rate(sampling_rate);
	if (ret < 0)
	{
		printf("Wrong sample rate %d\n", sampling_rate);
		return ret;
	}

	if ((1024 * 48000) % sampling_rate == 0)
		base_sampling_rate = 48000;
	else if ((1024 * 44100) % sampling_rate == 0)
		base_sampling_rate = 44100;
	else
	{
		printf("Wrong sample rate %d\n", sampling_rate);
		return -EINVAL;
	}

	i2c_fd = i2c_open(i2c_controller);
	if (i2c_fd > 0)
		printf("%s opened successfully\n", i2c_controller);
	else
	{
		printf("Unable to open %s\n", i2c_controller);
		perror("");
		return -ENODEV;
	}

	codec_i2c_address = i2c_addr;

	ret = calc_pll_values(freq_in, 1024 * base_sampling_rate, pll_regs);
	if (ret < 0)
	{
		printf("Wrong pll values\n");
		return ret;
	}

	ret = adau1761_write(0x4000, 0xe);		//core off
	if (ret < 0) goto err;

	ret = adau1761_pll_regs_write(pll_regs);
	if (ret < 0) goto err;

	usleep(10 * 1000);						//PLL lock time

	ret = adau1761_write(0x4000, 0xf);		//core on
	if (ret < 0) goto err;

	ret = adau1761_write(0x4015, (mode == CODEC_MODE_MASTER)? 1:0);		//master or slave
	if (ret < 0) goto err;

	for (i = 0; i < (sizeof(adau1761_config) / sizeof(adau1761_config[0])); i++)
	{
		ret = adau1761_write(adau1761_config[i].address, adau1761_config[i].data);
		if (ret < 0) goto err;
	}

	ret = adau1761_set_sample_rate(base_sampling_rate, sampling_rate);
	if(ret < 0)
	{
		printf("Failed to set sample rate\n");
		goto err;
	}

	printf("Codec initialized successfully\n");
err:
	printf("%s closed\n\n", i2c_controller);
	i2c_close(i2c_fd);

	return ret;
}

