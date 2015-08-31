/*
 * I2C access helper functions header
 *
 * Copyright (C) 2014 Xylon d.o.o.
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

#ifndef __I2C_ACCESS_H__
#define __I2C_ACCESS_H__

#include <stdint.h>
#include <linux/i2c.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef _Bool bool;
typedef struct i2c_msg i2c_msg_t;

void i2c_msg_set_read_mode(i2c_msg_t *msg);
void i2c_msg_set(i2c_msg_t *msg, uint16_t addr, bool rd, uint8_t *buf,
		 uint16_t len);
int i2c_msg_send(int fd, i2c_msg_t *msgs, int nmsgs);

int i2c_open(char *name);
void i2c_close(int fd);

#endif /* __I2C_ACCESS_H__ */
