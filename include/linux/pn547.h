/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * Copyright (C) 2019 Pavel Dubrova <pashadubrova@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _PN547_H_
#define _PN547_H_

#define PN547_DEVICE_NAME	"pn547"
#define PN547_MAGIC		0xE9

/*
 * PN547 power control via ioctl
 * PN547_SET_PWR(0): power off
 * PN547_SET_PWR(1): power on
 * PN547_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN547_SET_PWR		_IOW(PN547_MAGIC, 0x01, unsigned int)

struct pn547_i2c_platform_data {
	unsigned int irq_gpio;
	unsigned int ven_gpio;
	unsigned int firm_gpio;
	unsigned int pvdd_en_gpio;
	int gpio_fwdl_enable[4];
	int gpio_fwdl_disable[4];
};

enum pn547_state {
	PN547_STATE_UNKNOWN,
	PN547_STATE_OFF,
	PN547_STATE_ON,
	PN547_STATE_FWDL,
};
#endif
