/*
 * ADAU1761 codec initialization functions header
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
typedef enum {
	CODEC_MODE_SLAVE,
	CODEC_MODE_MASTER
} codec_mode;

int adau1761_check_sample_rate(unsigned long sample_rate);
int adau1761_init(char *i2c_controller, unsigned short int i2c_addr, int freq_in, codec_mode, int sampling_rate);
