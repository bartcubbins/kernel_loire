/*
 * Copyright (c) 2019, Pavel Dubrova <pashadubrova@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_MSM8976_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_MSM8976_H

#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>

#define VDD_DIG_FMAX_MAP1(l1, f1) \
	.vdd_class = &vdd_dig,				\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),			\
	},						\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP2(l1, f1, l2, f2) \
	.vdd_class = &vdd_dig,				\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),			\
		[VDD_DIG_##l2] = (f2),			\
	},						\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP3(l1, f1, l2, f2, l3, f3) \
	.vdd_class = &vdd_dig,				\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),			\
		[VDD_DIG_##l2] = (f2),			\
		[VDD_DIG_##l3] = (f3),			\
	},						\
	.num_rate_max = VDD_DIG_NUM

#define VDD_DIG_FMAX_MAP5(l1, f1, l2, f2, l3, f3, l4, f4, l5, f5) \
	.vdd_class = &vdd_dig,				\
	.rate_max = (unsigned long[VDD_DIG_NUM]) {	\
		[VDD_DIG_##l1] = (f1),			\
		[VDD_DIG_##l2] = (f2),			\
		[VDD_DIG_##l3] = (f3),			\
		[VDD_DIG_##l4] = (f4),			\
		[VDD_DIG_##l5] = (f5),			\
	},						\
	.num_rate_max = VDD_DIG_NUM

enum vdd_dig_levels {
	VDD_DIG_NONE,
	VDD_DIG_LOWER,
	VDD_DIG_LOW,
	VDD_DIG_NOMINAL,
	VDD_DIG_NOM_PLUS,
	VDD_DIG_HIGH,
	VDD_DIG_NUM
};

static int vdd_level[] = {
	RPM_REGULATOR_LEVEL_NONE,	/* VDD_DIG_NONE */
	RPM_REGULATOR_LEVEL_SVS,	/* VDD_DIG_LOWER  */
	RPM_REGULATOR_LEVEL_SVS_PLUS,	/* VDD_DIG_LOW */
	RPM_REGULATOR_LEVEL_NOM,	/* VDD_DIG_NOMINAL */
	RPM_REGULATOR_LEVEL_NOM_PLUS,	/* VDD_DIG_NOM_PLUS */
	RPM_REGULATOR_LEVEL_TURBO,	/* VDD_DIG_TURBO */
};

#endif
