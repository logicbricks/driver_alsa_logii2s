/*
 * I2C access helper functions
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

#include <error.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include "i2c.h"

void i2c_msg_set_read_mode(i2c_msg_t *msg)
{
	msg->flags |= I2C_M_RD;
}

void i2c_msg_set(i2c_msg_t *msg, uint16_t addr, bool rd, uint8_t *buf,
		 uint16_t len)
{
	memset(msg, 0, sizeof(*msg));

	msg->addr = addr >> 1;
	if (addr > (1 << 7))
		msg->flags |= I2C_M_TEN;
	if (rd)
		msg->flags |= I2C_M_RD;
	msg->len = len;
	msg->buf = buf;
}

int i2c_msg_send(int fd, i2c_msg_t *msgs, int nmsgs)
{
	struct i2c_rdwr_ioctl_data ioctl_data;

	ioctl_data.msgs = msgs;
	ioctl_data.nmsgs = nmsgs;

	return ioctl(fd, I2C_RDWR, &ioctl_data);
}

int i2c_open(char *name)
{
	return open(name, (O_RDWR | O_SYNC));
}

void i2c_close(int fd)
{
	close(fd);
}
