/*
 * Copyright (c) 2018, AngeloGioacchino Del Regno <kholk11@gmail.com>
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

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/qcom,gcc-msm8976.h>

#include "common.h"
#include "clk-regmap.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"
#include "clk-alpha-pll.h"
#include "clk-voter.h"

#include "vdd-level-msm8976.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_level);

enum {
	P_GPLL0_OUT_AUX,
	P_GPLL0_OUT_MAIN,
	P_GPLL0_OUT_M,
	P_GPLL2_OUT_MAIN,
	P_GPLL2_OUT_AUX,
	P_GPLL4_OUT_AUX,
	P_GPLL4_OUT_MAIN,
	P_GPLL6_OUT_AUX,
	P_GPLL6_OUT_MAIN,
	P_XO,
};

static const struct parent_map gcc_xo_map[] = {
	{ P_XO, 0 },
};
static const char * const gcc_xo[] = {
	"xo",
};

static const struct parent_map gcc_gpll0_map[] = {
	{ P_GPLL0_OUT_MAIN, 1 },
};
static const char * const gcc_gpll0[] = {
	"gpll0_out_main",
};

static const struct parent_map gcc_xo_gpll0_map[] = {
	{ P_XO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
};
static const char * const gcc_xo_gpll0[] = {
	"xo",
	"gpll0_out_main",
};

static const struct parent_map gcc_xo_gpll0a_map[] = {
	{ P_XO, 0 },
	{ P_GPLL0_OUT_AUX, 2 },
};

static const char * const gcc_xo_gpll0a[] = {
	"xo",
	"gpll0_out_main",
};


static const struct parent_map gcc_gpll0m_map[] = {
	{ P_GPLL0_OUT_M, 3 },
};
static const char * const gcc_gpll0m[] = {
	"gpll0_out_main",
};

static const struct parent_map gcc_xo_gpll0_gpll2_map[] = {
	{ P_XO, 0 },
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL2_OUT_MAIN, 4 },
};

static const char * const gcc_xo_gpll0_gpll2[] = {
	"xo",
	"gpll0_out_main",
	"gpll2_out_main",
};

static const struct parent_map gcc_gpll0_gpll2a_gpll4_map[] = {
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL2_OUT_AUX, 3 },
	{ P_GPLL4_OUT_MAIN, 2 },
};

static const char * const gcc_gpll0_gpll2a_gpll4[] = {
	"gpll0_out_main",
	"gpll2_out_main",
	"gpll4_out_main",
};

static const struct parent_map gcc_gpll0_gpll4_map[] = {
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL4_OUT_MAIN, 2 },
};

static const char * const gcc_gpll0_gpll4[] = {
	"gpll0_out_main",
	"gpll4_out_main",
};

static const struct parent_map gcc_gpll0_gpll6_map[] = {
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL6_OUT_MAIN, 3 },
};

static const char * const gcc_gpll0_gpll6[] = {
	"gpll0_out_main",
	"gpll6_out_main",
};

static const struct parent_map gcc_gpll0_gpll4a_map[] = {
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL4_OUT_AUX, 3 },
};

static const char * const gcc_gpll0_gpll4a[] = {
	"gpll0_out_main",
	"gpll4_out_main",
};

static const struct parent_map gcc_gpll0_gpll2a_gpll6a_map[] = {
	{ P_GPLL0_OUT_MAIN, 1 },
	{ P_GPLL2_OUT_AUX, 3 },
	{ P_GPLL6_OUT_AUX, 2 },
};

static const char * const gcc_gpll0_gpll2a_gpll6a[] = {
	"gpll0_out_main",
	"gpll2_out_main",
	"gpll6_out_main",
};

static const struct parent_map gcc_xo_gpll4_gpll6_map[] = {
	{ P_XO, 0 },
	{ P_GPLL4_OUT_MAIN, 2 },
	{ P_GPLL6_OUT_MAIN, 3 },
};
static const char * const gcc_xo_gpll4_gpll6[] = {
	"xo",
	"gpll4_out_main",
	"gpll6_out_main",
};

static const struct parent_map gcc_gpll6_map[] = {
	{ P_GPLL6_OUT_MAIN, 3 },
};

static const char * const gcc_gpll6[] = {
	"gpll6_out_main",
};

static struct clk_fixed_factor xo = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "xo",
		.parent_names = (const char *[]){ "cxo" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor xo_a = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "xo_a",
		.parent_names = (const char *[]){ "cxo_a" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_pll gpll0 = {
	.l_reg = 0x21004,
	.m_reg = 0x21008,
	.n_reg = 0x2100c,
	.config_reg = 0x21014,
	.mode_reg = 0x21000,
	.status_reg = 0x2101c,
	.status_bit = 17,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpll0",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

static struct clk_pll gpll2 = {
	.l_reg = 0x4A004,
	.m_reg = 0x4A008,
	.n_reg = 0x4A00c,
	.config_reg = 0x4A014,
	.mode_reg = 0x4A000,
	.status_reg = 0x4A01c,
	.status_bit = 17,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpll2",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

#define F_GPLL(f, l, m, n) { (f), (l), (m), (n), 0 }
static struct pll_freq_tbl gpll3_freq_tbl[] = {
	F_GPLL(1100000000, 57, 7, 24),
};

static struct clk_pll gpll3 = {
	.l_reg		= 0x22004,
	.m_reg		= 0x22008,
	.n_reg		= 0x2200c,
	.config_reg	= 0x22010,
	.mode_reg	= 0x22000,
	.status_reg	= 0x22024,
	.status_bit	= 17,
	.freq_tbl	= gpll3_freq_tbl,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpll3",
		.parent_names = (const char*[]) { "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

static struct clk_pll gpll4 = {
	.l_reg = 0x24004,
	.m_reg = 0x24008,
	.n_reg = 0x2400c,
	.config_reg = 0x24018,
	.mode_reg = 0x24000,
	.status_reg = 0x24024,
	.status_bit = 17,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpll4",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

static struct clk_pll gpll6 = {
	.mode_reg = 0x37000,
	.l_reg = 0x37004,
	.m_reg = 0x37008,
	.n_reg = 0x3700c,
	.config_reg = 0x37014,
	.status_reg = 0x3701c,
	.status_bit = 17,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpll6",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

static unsigned int gpll0_voter;

static struct clk_pll_acpu_vote gpll0_ao_out_main = {
	.soft_voter = &gpll0_voter,
	.soft_voter_mask = PLL_SOFT_VOTE_CPU,
	.clkr = {
		.enable_reg = 0x45000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gpll0_ao_out_main",
			.parent_names = (const char *[]){ "gpll0" },
			.num_parents = 1,
			.ops = &clk_pll_vote_ops,
		},
	},
};

static struct clk_pll_acpu_vote gpll0_out_main = {
	.soft_voter = &gpll0_voter,
	.soft_voter_mask = PLL_SOFT_VOTE_PRIMARY,
	.clkr = {
		.enable_reg = 0x45000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gpll0_out_main",
			.parent_names = (const char *[])
					{ "gpll0" },
			.num_parents = 1,
			.ops = &clk_pll_vote_ops,
		},
	},
};

static struct clk_regmap gpll2_out_main = {
	.enable_reg = 0x45000,
	.enable_mask = BIT(2),
	.hw.init = &(struct clk_init_data) {
		.name = "gpll2_out_main",
		.parent_names = (const char *[]){ "gpll2" },
		.num_parents = 1,
		.ops = &clk_pll_vote_ops,
	},
};

/* GPLL3 at 1100MHz, main output enabled. */
static struct pll_config gpll3_config = {
	.l = 57,
	.m = 7,
	.n = 24,
	.vco_val = 0x0,
	.vco_mask = 0x3 << 20,
	.pre_div_val = 0x0,
	.pre_div_mask = 0x7 << 12,
	.post_div_val = 0x0,
	.post_div_mask = 0x3 << 8,
	.mn_ena_val = BIT(24),
	.mn_ena_mask = BIT(24),
	.main_output_mask = BIT(0),
	.aux_output_mask = BIT(1),
};

static struct clk_regmap gpll3_out_main = {
	.enable_reg = 0x45000,
	.enable_mask = BIT(4),
	.hw.init = &(struct clk_init_data){
		.name = "gpll3_out_main",
		.parent_names = (const char *[]){ "gpll3" },
		.num_parents = 1,
		.ops = &clk_pll_vote_ops,
	},
};

static struct clk_regmap gpll4_out_main = {
	.enable_reg = 0x45000,
	.enable_mask = BIT(5),
	.hw.init = &(struct clk_init_data) {
		.name = "gpll4_out_main",
		.parent_names = (const char *[]){ "gpll4" },
		.num_parents = 1,
		.ops = &clk_pll_vote_ops,
	},
};

static struct clk_regmap gpll6_out_main = {
	.enable_reg = 0x45000,
	.enable_mask = BIT(7),
	.hw.init = &(struct clk_init_data) {
		.name = "gpll6_out_main",
		.parent_names = (const char *[]){ "gpll6" },
		.num_parents = 1,
		.flags = CLK_ENABLE_HAND_OFF,
		.ops = &clk_pll_vote_ops,
	},
};

static const struct freq_tbl ftbl_aps_0_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	F( 300000000,          P_GPLL4_OUT_MAIN,    4,    0,     0),
	F( 540000000,          P_GPLL6_OUT_MAIN,    2,    0,     0),
	{ }
};

static struct clk_rcg2 aps_0_clk_src = {
	.cmd_rcgr = 0x78008,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll4_gpll6_map,
	.freq_tbl = ftbl_aps_0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "aps_0_clk_src",
		.parent_names = gcc_xo_gpll4_gpll6,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll4_gpll6),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 300000000, NOMINAL, 540000000),
	},
};

static const struct freq_tbl ftbl_aps_1_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	F( 300000000,          P_GPLL4_OUT_MAIN,    4,    0,     0),
	F( 540000000,          P_GPLL6_OUT_MAIN,    2,    0,     0),
	{ }
};

static struct clk_rcg2 aps_1_clk_src = {
	.cmd_rcgr = 0x79008,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll4_gpll6_map,
	.freq_tbl = ftbl_aps_1_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "aps_1_clk_src",
		.parent_names = gcc_xo_gpll4_gpll6,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll4_gpll6),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 300000000, NOMINAL, 540000000),
	},
};

static const struct freq_tbl ftbl_apss_ahb_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F(  88890000,          P_GPLL0_OUT_MAIN,    9,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	{ }
};

static struct clk_rcg2 apss_ahb_clk_src = {
	.cmd_rcgr = 0x46000,
//	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_apss_ahb_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "apss_ahb_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.flags = CLK_ENABLE_HAND_OFF,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_blsp_i2c_apps_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	{ }
};

static struct clk_rcg2 blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0200c,
//	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup1_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static const struct freq_tbl ftbl_blsp_spi_apps_clk_src[] = {
	F(    960000,                      P_XO,   10,    1,     2),
	F(   4800000,                      P_XO,    4,    0,     0),
	F(   9600000,                      P_XO,    2,    0,     0),
	F(  16000000,          P_GPLL0_OUT_MAIN,   10,    1,     5),
	F(  19200000,                      P_XO,    1,    0,     0),
	F(  25000000,          P_GPLL0_OUT_MAIN,   16,    1,     2),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	{ }
};

static struct clk_rcg2 blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr = 0x02024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup1_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr = 0x03000,
//	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup2_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr = 0x03014,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup2_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static struct clk_rcg2 blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr = 0x4000,
//	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup3_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr = 0x4024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup3_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static struct clk_rcg2 blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr = 0x5000,
//	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup4_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr = 0x5024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_qup4_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static const struct freq_tbl ftbl_blsp_uart_apps_clk_src[] = {
	F(   3686400,          P_GPLL0_OUT_MAIN,    1,   72, 15625),
	F(   7372800,          P_GPLL0_OUT_MAIN,    1,  144, 15625),
	F(  14745600,          P_GPLL0_OUT_MAIN,    1,  288, 15625),
	F(  16000000,          P_GPLL0_OUT_MAIN,   10,    1,     5),
	F(  19200000,                      P_XO,    1,    0,     0),
	F(  24000000,          P_GPLL0_OUT_MAIN,    1,    3,   100),
	F(  25000000,          P_GPLL0_OUT_MAIN,   16,    1,     2),
	F(  32000000,          P_GPLL0_OUT_MAIN,    1,    1,    25),
	F(  40000000,          P_GPLL0_OUT_MAIN,    1,    1,    20),
	F(  46400000,          P_GPLL0_OUT_MAIN,    1,   29,   500),
	F(  48000000,          P_GPLL0_OUT_MAIN,    1,    3,    50),
	F(  51200000,          P_GPLL0_OUT_MAIN,    1,    8,   125),
	F(  56000000,          P_GPLL0_OUT_MAIN,    1,    7,   100),
	F(  58982400,          P_GPLL0_OUT_MAIN,    1, 1152, 15625),
	F(  60000000,          P_GPLL0_OUT_MAIN,    1,    3,    40),
	F(  64000000,          P_GPLL0_OUT_MAIN,    1,    2,    25),
	{ }
};

static struct clk_rcg2 blsp1_uart1_apps_clk_src = {
	.cmd_rcgr = 0x2044,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_uart1_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
	},
};

static struct clk_rcg2 blsp1_uart2_apps_clk_src = {
	.cmd_rcgr = 0x3034,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp1_uart2_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
	},
};

static struct clk_rcg2 blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0c00c,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup1_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr = 0x0c024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup1_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0d000,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup2_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr = 0x0d014,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup2_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0f000,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup3_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr = 0x0f024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup3_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr = 0x18000,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_i2c_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup4_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 50000000),
	},
};

static struct clk_rcg2 blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr = 0x18024,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_spi_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_qup4_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 25000000, NOMINAL, 50000000),
	},
};

static struct clk_rcg2 blsp2_uart1_apps_clk_src = {
	.cmd_rcgr = 0x0c044,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_uart1_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
	},
};

static struct clk_rcg2 blsp2_uart2_apps_clk_src = {
	.cmd_rcgr = 0x0d034,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_blsp_uart_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "blsp2_uart2_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 32000000, NOMINAL, 64000000),
	},
};

static const struct freq_tbl ftbl_camss_gp0_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 camss_gp0_clk_src = {
	.cmd_rcgr = 0x54000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_camss_gp0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "camss_gp0_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, NOMINAL, 200000000,
		HIGH, 266670000),
	},
};

static const struct freq_tbl ftbl_camss_gp1_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 camss_gp1_clk_src = {
	.cmd_rcgr = 0x55000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_camss_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "camss_gp1_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, NOMINAL, 200000000,
		HIGH, 266670000),
	},
};

static const struct freq_tbl ftbl_camss_top_ahb_clk_src[] = {
	F(  40000000,          P_GPLL0_OUT_MAIN,   10,    1,     2),
	F(  80000000,          P_GPLL0_OUT_MAIN,   10,    0,     0),
	{ }
};

static struct clk_rcg2 camss_top_ahb_clk_src = {
	.cmd_rcgr = 0x5a000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_camss_top_ahb_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "camss_top_ahb_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 40000000, LOW, 80000000),
	},
};

static const struct freq_tbl ftbl_cci_clk_src[] = {
	F(  19200000,                 P_XO,    1,    0,     0),
	F(  37500000,      P_GPLL0_OUT_AUX,    1,    3,    64),
	{ }
};

static struct clk_rcg2 cci_clk_src = {
	.cmd_rcgr = 0x51000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0a_map,
	.freq_tbl = ftbl_cci_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "cci_clk_src",
		.parent_names = gcc_xo_gpll0a,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0a),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 19200000, NOMINAL, 37500000),
	},
};

static const struct freq_tbl ftbl_cpp_clk_src[] = {
	F( 160000000,          P_GPLL0_OUT_MAIN,    5,    0,     0),
	F( 240000000,           P_GPLL4_OUT_AUX,    5,    0,     0),
	F( 320000000,          P_GPLL0_OUT_MAIN,  2.5,    0,     0),
	F( 400000000,          P_GPLL0_OUT_MAIN,    2,    0,     0),
	F( 480000000,           P_GPLL4_OUT_AUX,  2.5,    0,     0),
	{ }
};

static struct clk_rcg2 cpp_clk_src = {
	.cmd_rcgr = 0x58018,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll4a_map,
	.freq_tbl = ftbl_cpp_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "cpp_clk_src",
		.parent_names = gcc_gpll0_gpll4a,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll4a),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(LOWER, 160000000, LOW, 240000000,
		NOMINAL, 320000000, NOM_PLUS, 400000000, HIGH, 480000000),
	},
};

static const struct freq_tbl ftbl_crypto_clk_src[] = {
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F(  80000000,          P_GPLL0_OUT_MAIN,   10,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 160000000,          P_GPLL0_OUT_MAIN,    5,    0,     0),
	{ }
};

static struct clk_rcg2 crypto_clk_src = {
	.cmd_rcgr = 0x16004,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_crypto_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "crypto_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 80000000, NOMINAL, 160000000),
	},
};

static const struct freq_tbl ftbl_csi0_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 csi0_clk_src = {
	.cmd_rcgr = 0x4e020,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_csi0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "csi0_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
		NOM_PLUS, 266670000),
	},
};

static const struct freq_tbl ftbl_csi1_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 csi1_clk_src = {
	.cmd_rcgr = 0x4f020,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_csi1_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "csi1_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
		NOM_PLUS, 266670000),
	},
};

static const struct freq_tbl ftbl_csi2_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 csi2_clk_src = {
	.cmd_rcgr = 0x3c020,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_csi2_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "csi2_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
		NOM_PLUS, 266670000),
	},
};

static const struct freq_tbl ftbl_csi0phytimer_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 csi0phytimer_clk_src = {
	.cmd_rcgr = 0x4e000,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_csi0phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "csi0phytimer_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
		NOM_PLUS, 266670000),
	},
};

static const struct freq_tbl ftbl_csi1phytimer_clk_src[] = {
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266670000,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 csi1phytimer_clk_src = {
	.cmd_rcgr = 0x4f000,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_csi1phytimer_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "csi1phytimer_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 100000000, LOW, 200000000,
		NOM_PLUS, 266670000),
	},
};

static const struct freq_tbl ftbl_gp1_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	{ }
};

static struct clk_rcg2 gp1_clk_src = {
	.cmd_rcgr = 0x08004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_map,
	.freq_tbl = ftbl_gp1_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gp1_clk_src",
		.parent_names = gcc_xo,
		.num_parents = ARRAY_SIZE(gcc_xo),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_gp2_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	{ }
};

static struct clk_rcg2 gp2_clk_src = {
	.cmd_rcgr = 0x09004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_map,
	.freq_tbl = ftbl_gp2_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gp2_clk_src",
		.parent_names = gcc_xo,
		.num_parents = ARRAY_SIZE(gcc_xo),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_gp3_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	{ }
};

static struct clk_rcg2 gp3_clk_src = {
	.cmd_rcgr = 0x0a004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_map,
	.freq_tbl = ftbl_gp3_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gp3_clk_src",
		.parent_names = gcc_xo,
		.num_parents = ARRAY_SIZE(gcc_xo),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_jpeg0_clk_src[] = {
	F( 133330000,          P_GPLL0_OUT_MAIN,    6,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266666667,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	F( 300000000,          P_GPLL4_OUT_MAIN,    4,    0,     0),
	F( 320000000,          P_GPLL0_OUT_MAIN,  2.5,    0,     0),
	{ }
};

static struct clk_rcg2 jpeg0_clk_src = {
	.cmd_rcgr = 0x57000,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll4_map,
	.freq_tbl = ftbl_jpeg0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "jpeg0_clk_src",
		.parent_names = gcc_gpll0_gpll4,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll4),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(LOWER, 133330000, LOW, 200000000,
		NOMINAL, 266670000, NOM_PLUS, 300000000,
		HIGH, 320000000),
	},
};

static const struct freq_tbl ftbl_mclk_clk_src[] = {
	F(  24000000,          P_GPLL6_OUT_MAIN,    1,    1,    45),
	F(  66670000,          P_GPLL0_OUT_MAIN,   12,    0,     0),
	{ }
};

static struct clk_rcg2 mclk0_clk_src = {
	.cmd_rcgr = 0x52000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll6_map,
	.freq_tbl = ftbl_mclk_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "mclk0_clk_src",
		.parent_names = gcc_gpll0_gpll6,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll6),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
	},
};

static struct clk_rcg2 mclk1_clk_src = {
	.cmd_rcgr = 0x53000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll6_map,
	.freq_tbl = ftbl_mclk_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "mclk1_clk_src",
		.parent_names = gcc_gpll0_gpll6,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll6),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
	},
};

static struct clk_rcg2 mclk2_clk_src = {
	.cmd_rcgr = 0x5c000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll6_map,
	.freq_tbl = ftbl_mclk_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "mclk2_clk_src",
		.parent_names = gcc_gpll0_gpll6,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll6),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 66670000),
	},
};

static const struct freq_tbl ftbl_pdm2_clk_src[] = {
	F(  64000000,          P_GPLL0_OUT_MAIN, 12.5,    0,     0),
	{ }
};

static struct clk_rcg2 pdm2_clk_src = {
	.cmd_rcgr = 0x44010,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_pdm2_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "pdm2_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 64000000),
	},
};

static const struct freq_tbl ftbl_rbcpr_gfx_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	{ }
};

static struct clk_rcg2 rbcpr_gfx_clk_src = {
	.cmd_rcgr = 0x3a00c,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_rbcpr_gfx_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "rbcpr_gfx_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 100000000),
	},
};

static const struct freq_tbl ftbl_sdcc1_apps_clk_src[] = {
	F(    144000,                      P_XO,   16,    3,    25),
	F(    400000,                      P_XO,   12,    1,     4),
	F(  20000000,          P_GPLL0_OUT_MAIN,   10,    1,     4),
	F(  25000000,          P_GPLL0_OUT_MAIN,   16,    1,     2),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 177777778,          P_GPLL0_OUT_MAIN,  4.5,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 342850000,          P_GPLL4_OUT_MAIN,  3.5,    0,     0),
	F( 400000000,          P_GPLL4_OUT_MAIN,    3,    0,     0),
	{ }
};

static const struct freq_tbl ftbl_sdcc1_v1_apps_clk_src[] = {
	F(    144000,                      P_XO,   16,    3,    25),
	F(    400000,                      P_XO,   12,    1,     4),
	F(  20000000,          P_GPLL0_OUT_MAIN,   10,    1,     4),
	F(  25000000,          P_GPLL0_OUT_MAIN,   16,    1,     2),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 177777778,          P_GPLL0_OUT_MAIN,  4.5,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 186400000,          P_GPLL2_OUT_MAIN,    5,    0,     0),
	F( 372800000,          P_GPLL2_OUT_MAIN,  2.5,    0,     0),
	{ }
};

static struct clk_rcg2 sdcc1_apps_clk_src = {
	.cmd_rcgr = 0x42004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_gpll2_map,
	.freq_tbl = ftbl_sdcc1_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "sdcc1_apps_clk_src",
		.parent_names = gcc_xo_gpll0_gpll2,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0_gpll2),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 400000000),
	},
};

static const struct freq_tbl ftbl_sdcc1_ice_core_clk_src[] = {
	F( 100000000,             P_GPLL0_OUT_M,    8,    0,     0),
	F( 200000000,             P_GPLL0_OUT_M,    4,    0,     0),
	{ }
};

static struct clk_rcg2 sdcc1_ice_core_clk_src = {
	.cmd_rcgr = 0x5d000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0m_map,
	.freq_tbl = ftbl_sdcc1_ice_core_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "sdcc1_ice_core_clk_src",
		.parent_names = gcc_gpll0m,
		.num_parents = ARRAY_SIZE(gcc_gpll0m),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 100000000, NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_sdcc2_3_apps_clk_src[] = {
	F(    144000,                      P_XO,   16,    3,    25),
	F(    400000,                      P_XO,   12,    1,     4),
	F(  20000000,          P_GPLL0_OUT_MAIN,   10,    1,     4),
	F(  25000000,          P_GPLL0_OUT_MAIN,   16,    1,     2),
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 177777778,          P_GPLL0_OUT_MAIN,  4.5,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	{ }
};

static struct clk_rcg2 sdcc2_apps_clk_src = {
	.cmd_rcgr = 0x43004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_sdcc2_3_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "sdcc2_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 200000000),
	},
};

static struct clk_rcg2 sdcc3_apps_clk_src = {
	.cmd_rcgr = 0x39004,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_sdcc2_3_apps_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "sdcc3_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP2(LOWER, 50000000, NOMINAL, 200000000),
	},
};

static const struct freq_tbl ftbl_vcodec0_clk_src[] = {
	F(  72727200,          P_GPLL0_OUT_MAIN,   11,    0,     0),
	F(  80000000,          P_GPLL0_OUT_MAIN,   10,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 133333333,          P_GPLL0_OUT_MAIN,    6,    0,     0),
	F( 228570000,          P_GPLL0_OUT_MAIN,  3.5,    0,     0),
	F( 310667000,           P_GPLL2_OUT_AUX,    3,    0,     0),
	F( 360000000,           P_GPLL6_OUT_AUX,    3,    0,     0),
	F( 400000000,          P_GPLL0_OUT_MAIN,    2,    0,     0),
	F( 466000000,           P_GPLL2_OUT_AUX,    2,    0,     0),
	{ }
};

static struct clk_rcg2 vcodec0_clk_src = {
	.cmd_rcgr = 0x4c000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll2a_gpll6a_map,
	.freq_tbl = ftbl_vcodec0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "vcodec0_clk_src",
		.parent_names = gcc_gpll0_gpll2a_gpll6a,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll2a_gpll6a),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(LOWER, 228570000, LOW, 310667000,
		NOMINAL, 360000000, NOM_PLUS, 400000000,
		HIGH, 466000000),
	},
};

static const struct freq_tbl ftbl_vfe0_clk_src[] = {
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F(  80000000,          P_GPLL0_OUT_MAIN,   10,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 133333333,          P_GPLL0_OUT_MAIN,    6,    0,     0),
	F( 160000000,          P_GPLL0_OUT_MAIN,    5,    0,     0),
	F( 177777778,          P_GPLL0_OUT_MAIN,  4.5,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266666667,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	F( 300000000,          P_GPLL4_OUT_MAIN,    4,    0,     0),
	F( 320000000,          P_GPLL0_OUT_MAIN,  2.5,    0,     0),
	F( 400000000,          P_GPLL0_OUT_MAIN,    2,    0,     0),
	F( 466000000,           P_GPLL2_OUT_AUX,    2,    0,     0),
	{ }
};

static struct clk_rcg2 vfe0_clk_src = {
	.cmd_rcgr = 0x58000,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll2a_gpll4_map,
	.freq_tbl = ftbl_vfe0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "vfe0_clk_src",
		.parent_names = gcc_gpll0_gpll2a_gpll4,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll2a_gpll4),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(LOWER, 160000000, LOW, 300000000,
		NOMINAL, 320000000, NOM_PLUS, 400000000,
		HIGH, 466000000),
	},
};

static const struct freq_tbl ftbl_vfe1_clk_src[] = {
	F(  50000000,          P_GPLL0_OUT_MAIN,   16,    0,     0),
	F(  80000000,          P_GPLL0_OUT_MAIN,   10,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 133333333,          P_GPLL0_OUT_MAIN,    6,    0,     0),
	F( 160000000,          P_GPLL0_OUT_MAIN,    5,    0,     0),
	F( 177777778,          P_GPLL0_OUT_MAIN,  4.5,    0,     0),
	F( 200000000,          P_GPLL0_OUT_MAIN,    4,    0,     0),
	F( 266666667,          P_GPLL0_OUT_MAIN,    3,    0,     0),
	F( 300000000,          P_GPLL4_OUT_MAIN,    4,    0,     0),
	F( 320000000,          P_GPLL0_OUT_MAIN,  2.5,    0,     0),
	F( 400000000,          P_GPLL0_OUT_MAIN,    2,    0,     0),
	F( 466000000,          P_GPLL2_OUT_AUX,    2,    0,     0),
	{ }
};

static struct clk_rcg2 vfe1_clk_src = {
	.cmd_rcgr = 0x58054,
	.hid_width = 5,
	.parent_map = gcc_gpll0_gpll2a_gpll4_map,
	.freq_tbl = ftbl_vfe1_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "vfe1_clk_src",
		.parent_names = gcc_gpll0_gpll2a_gpll4,
		.num_parents = ARRAY_SIZE(gcc_gpll0_gpll2a_gpll4),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP5(LOWER, 160000000, LOW, 300000000,
		NOMINAL, 320000000, NOM_PLUS, 400000000,
		HIGH, 466000000),
	},
};

static const struct freq_tbl ftbl_usb_fs_ic_clk_src[] = {
	F(  60000000,          P_GPLL6_OUT_MAIN,    6,    1,     3),
	{ }
};

static struct clk_rcg2 usb_fs_ic_clk_src = {
	.cmd_rcgr = 0x3f034,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll6_map,
	.freq_tbl = ftbl_usb_fs_ic_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "usb_fs_ic_clk_src",
		.parent_names = gcc_gpll6,
		.num_parents = ARRAY_SIZE(gcc_gpll6),
		.ops = &clk_rcg2_ops,
		.flags = CLK_ENABLE_HAND_OFF,
		VDD_DIG_FMAX_MAP1(LOWER, 60000000),
	},
};

static const struct freq_tbl ftbl_usb_fs_system_clk_src[] = {
	F(  64000000,      P_GPLL0_OUT_MAIN, 12.5,    0,     0),
	{ }
};

static struct clk_rcg2 usb_fs_system_clk_src = {
	.cmd_rcgr = 0x3f010,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_usb_fs_system_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "usb_fs_system_clk_src",
		.parent_names = gcc_gpll0,
		.num_parents = ARRAY_SIZE(gcc_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 64000000),
	},
};

static const struct freq_tbl ftbl_usb_hs_system_clk_src[] = {
	F(  57140000,          P_GPLL0_OUT_MAIN,   14,    0,     0),
	F( 100000000,          P_GPLL0_OUT_MAIN,    8,    0,     0),
	F( 133330000,          P_GPLL0_OUT_MAIN,    6,    0,     0),
	F( 177780000,          P_GPLL0_OUT_MAIN,  4.5,    0,     0),
	{ }
};

static struct clk_rcg2 usb_hs_system_clk_src = {
	.cmd_rcgr = 0x41010,
	.hid_width = 5,
	.parent_map = gcc_gpll0_map,
	.freq_tbl = ftbl_usb_hs_system_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "usb_hs_system_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = ARRAY_SIZE(gcc_xo_gpll0),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP3(LOWER, 57140000, NOMINAL, 133330000,
		HIGH, 177780000),
	},
};

static struct clk_branch gcc_aps_0_clk = {
	.halt_reg = 0x78004,
	.clkr = {
		.enable_reg = 0x78004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_aps_0_clk",
			.parent_names = (const char*[]) {
				"aps_0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_aps_1_clk = {
	.halt_reg = 0x79004,
	.clkr = {
		.enable_reg = 0x79004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_aps_1_clk",
			.parent_names = (const char*[]) {
				"aps_1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_apss_ahb_clk = {
	.halt_reg = 0x4601c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(14),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_apss_ahb_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_apss_axi_clk = {
	.halt_reg = 0x46020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(13),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_apss_axi_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_apss_tcu_clk = {
	.halt_reg = 0x12018,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_apss_tcu_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_ahb_clk = {
	.halt_reg = 0x01008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(10),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_ahb_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_ahb_clk = {
	.halt_reg = 0x0b008,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(20),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup1_spi_apps_clk = {
	.halt_reg = 0x02004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x02004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup1_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup1_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup1_i2c_apps_clk = {
	.halt_reg = 0x02008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x02008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup1_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup1_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup2_i2c_apps_clk = {
	.halt_reg = 0x03010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x03010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup2_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup2_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup2_spi_apps_clk = {
	.halt_reg = 0x0300C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0300C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup2_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup2_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup3_i2c_apps_clk = {
	.halt_reg = 0x04020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x04020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup3_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup3_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup3_spi_apps_clk = {
	.halt_reg = 0x0401C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0401C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup3_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup3_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup4_i2c_apps_clk = {
	.halt_reg = 0x05020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x05020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup4_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup4_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup4_spi_apps_clk = {
	.halt_reg = 0x0501C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0501C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_qup4_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_qup4_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart1_apps_clk = {
	.halt_reg = 0x0203C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0203C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_uart1_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_uart1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart2_apps_clk = {
	.halt_reg = 0x0302C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0302C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp1_uart2_apps_clk",
			.parent_names = (const char*[]) {
				"blsp1_uart2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup1_i2c_apps_clk = {
	.halt_reg = 0x0C008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0C008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup1_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup1_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup1_spi_apps_clk = {
	.halt_reg = 0x0C004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0C004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup1_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup1_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup2_i2c_apps_clk = {
	.halt_reg = 0x0D010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0D010,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup2_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup2_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup2_spi_apps_clk = {
	.halt_reg = 0x0D00C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0D00C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup2_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup2_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup3_i2c_apps_clk = {
	.halt_reg = 0x0F020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0F020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup3_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup3_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup3_spi_apps_clk = {
	.halt_reg = 0x0F01C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0F01C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup3_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup3_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup4_i2c_apps_clk = {
	.halt_reg = 0x18020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x18020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup4_i2c_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup4_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup4_spi_apps_clk = {
	.halt_reg = 0x1801C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1801C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_qup4_spi_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_qup4_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart1_apps_clk = {
	.halt_reg = 0x0C03C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0C03C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_uart1_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_uart1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart2_apps_clk = {
	.halt_reg = 0x0D02C,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0D02C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_blsp2_uart2_apps_clk",
			.parent_names = (const char*[]) {
				"blsp2_uart2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_boot_rom_ahb_clk = {
	.halt_reg = 0x1300c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(7),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_boot_rom_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_ahb_clk = {
	.halt_reg = 0x56004,
	.clkr = {
		.enable_reg = 0x56004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_cci_ahb_clk = {
	.halt_reg = 0x5101c,
	.clkr = {
		.enable_reg = 0x5101c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_cci_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_cci_clk = {
	.halt_reg = 0x51018,
	.clkr = {
		.enable_reg = 0x51018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_cci_clk",
			.parent_names = (const char*[]) {
				"cci_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_cpp_ahb_clk = {
	.halt_reg = 0x58040,
	.clkr = {
		.enable_reg = 0x58040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_cpp_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_cpp_axi_clk = {
	.halt_reg = 0x58064,
	.clkr = {
		.enable_reg = 0x58064,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_cpp_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_cpp_clk = {
	.halt_reg = 0x5803c,
	.clkr = {
		.enable_reg = 0x5803c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_cpp_clk",
			.parent_names = (const char*[]) {
				"cpp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi0_ahb_clk = {
	.halt_reg = 0x4e040,
	.clkr = {
		.enable_reg = 0x4e040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi0_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi0_clk = {
	.halt_reg = 0x4e03c,
	.clkr = {
		.enable_reg = 0x4e03c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi0_clk",
			.parent_names = (const char*[]) {
				"csi0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi0phy_clk = {
	.halt_reg = 0x4e048,
	.clkr = {
		.enable_reg = 0x4e048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi0phy_clk",
			.parent_names = (const char*[]) {
				"csi0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi0pix_clk = {
	.halt_reg = 0x4e058,
	.clkr = {
		.enable_reg = 0x4e058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi0pix_clk",
			.parent_names = (const char*[]) {
				"csi0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi0rdi_clk = {
	.halt_reg = 0x4e050,
	.clkr = {
		.enable_reg = 0x4e050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi0rdi_clk",
			.parent_names = (const char*[]) {
				"csi0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi1_ahb_clk = {
	.halt_reg = 0x4f040,
	.clkr = {
		.enable_reg = 0x4f040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi1_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi1_clk = {
	.halt_reg = 0x4F03C,
	.clkr = {
		.enable_reg = 0x4F03C,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi1_clk",
			.parent_names = (const char*[]) {
				"csi1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi1phy_clk = {
	.halt_reg = 0x4f048,
	.clkr = {
		.enable_reg = 0x4f048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi1phy_clk",
			.parent_names = (const char*[]) {
				"csi1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi1pix_clk = {
	.halt_reg = 0x4f058,
	.clkr = {
		.enable_reg = 0x4f058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi1pix_clk",
			.parent_names = (const char*[]) {
				"csi1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi1rdi_clk = {
	.halt_reg = 0x4f050,
	.clkr = {
		.enable_reg = 0x4f050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi1rdi_clk",
			.parent_names = (const char*[]) {
				"csi1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi2_ahb_clk = {
	.halt_reg = 0x3c040,
	.clkr = {
		.enable_reg = 0x3c040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi2_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi2_clk = {
	.halt_reg = 0x3c03c,
	.clkr = {
		.enable_reg = 0x3c03c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi2_clk",
			.parent_names = (const char*[]) {
				"csi2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi2phy_clk = {
	.halt_reg = 0x3c048,
	.clkr = {
		.enable_reg = 0x3c048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi2phy_clk",
			.parent_names = (const char*[]) {
				"csi2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi2pix_clk = {
	.halt_reg = 0x3c058,
	.clkr = {
		.enable_reg = 0x3c058,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi2pix_clk",
			.parent_names = (const char*[]) {
				"csi2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi2rdi_clk = {
	.halt_reg = 0x3c050,
	.clkr = {
		.enable_reg = 0x3c050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi2rdi_clk",
			.parent_names = (const char*[]) {
				"csi2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi_vfe0_clk = {
	.halt_reg = 0x58050,
	.clkr = {
		.enable_reg = 0x58050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi_vfe0_clk",
			.parent_names = (const char*[]) {
				"vfe0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi_vfe1_clk = {
	.halt_reg = 0x58074,
	.clkr = {
		.enable_reg = 0x58074,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi_vfe1_clk",
			.parent_names = (const char*[]) {
				"vfe1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi0phytimer_clk = {
	.halt_reg = 0x4e01c,
	.clkr = {
		.enable_reg = 0x4e01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi0phytimer_clk",
			.parent_names = (const char*[]) {
				"csi0phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_csi1phytimer_clk = {
	.halt_reg = 0x4f01c,
	.clkr = {
		.enable_reg = 0x4f01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_csi1phytimer_clk",
			.parent_names = (const char*[]) {
				"csi1phytimer_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_gp0_clk = {
	.halt_reg = 0x54018,
	.clkr = {
		.enable_reg = 0x54018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_gp0_clk",
			.parent_names = (const char*[]) {
				"camss_gp0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_gp1_clk = {
	.halt_reg = 0x55018,
	.clkr = {
		.enable_reg = 0x55018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_gp1_clk",
			.parent_names = (const char*[]) {
				"camss_gp1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_ispif_ahb_clk = {
	.halt_reg = 0x50004,
	.clkr = {
		.enable_reg = 0x50004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_ispif_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_jpeg0_clk = {
	.halt_reg = 0x57020,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x57020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_jpeg0_clk",
			.parent_names = (const char*[]) {
				"jpeg0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_jpeg_ahb_clk = {
	.halt_reg = 0x57024,
	.clkr = {
		.enable_reg = 0x57024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_jpeg_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_jpeg_axi_clk = {
	.halt_reg = 0x57028,
	.clkr = {
		.enable_reg = 0x57028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_jpeg_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_mclk0_clk = {
	.halt_reg = 0x52018,
	.clkr = {
		.enable_reg = 0x52018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_mclk0_clk",
			.parent_names = (const char*[]) {
				"mclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_mclk1_clk = {
	.halt_reg = 0x53018,
	.clkr = {
		.enable_reg = 0x53018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_mclk1_clk",
			.parent_names = (const char*[]) {
				"mclk1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_mclk2_clk = {
	.halt_reg = 0x5c018,
	.clkr = {
		.enable_reg = 0x5c018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_mclk2_clk",
			.parent_names = (const char*[]) {
				"mclk2_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_micro_ahb_clk = {
	.halt_reg = 0x5600c,
	.clkr = {
		.enable_reg = 0x5600c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_micro_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_top_ahb_clk = {
	.halt_reg = 0x5a014,
	.clkr = {
		.enable_reg = 0x5a014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_top_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_vfe0_clk = {
	.halt_reg = 0x58038,
	.clkr = {
		.enable_reg = 0x58038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_vfe0_clk",
			.parent_names = (const char*[]) {
				"vfe0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_vfe_ahb_clk = {
	.halt_reg = 0x58044,
	.clkr = {
		.enable_reg = 0x58044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_vfe_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_vfe_axi_clk = {
	.halt_reg = 0x58048,
	.clkr = {
		.enable_reg = 0x58048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_vfe_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_vfe1_clk = {
	.halt_reg = 0x5805c,
	.clkr = {
		.enable_reg = 0x5805c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_vfe1_clk",
			.parent_names = (const char*[]) {
				"vfe1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_vfe1_ahb_clk = {
	.halt_reg = 0x58060,
	.clkr = {
		.enable_reg = 0x58060,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_vfe1_ahb_clk",
			.parent_names = (const char*[]) {
				"camss_top_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_camss_vfe1_axi_clk = {
	.halt_reg = 0x58068,
	.clkr = {
		.enable_reg = 0x58068,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_camss_vfe1_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_cpp_tbu_clk = {
	.halt_reg = 0x12040,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(14),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_cpp_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_crypto_ahb_clk = {
	.halt_reg = 0x16024,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_crypto_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_crypto_axi_clk = {
	.halt_reg = 0x16020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(1),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_crypto_axi_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_crypto_clk = {
	.halt_reg = 0x1601C,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(2),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_crypto_clk",
			.parent_names = (const char *[]){
				"crypto_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_dcc_clk = {
	.halt_reg = 0x77004,
	.clkr = {
		.enable_reg = 0x77004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_dcc_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_fs_system_clk = {
	.halt_reg = 0x3f004,
	.clkr = {
		.enable_reg = 0x3f004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb_fs_system_clk",
			.parent_names = (const char*[]) {
				"usb_fs_system_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp1_clk = {
	.halt_reg = 0x08000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x08000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_gp1_clk",
			.parent_names = (const char*[]) {
				"gp1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp2_clk = {
	.halt_reg = 0x09000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x09000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_gp2_clk",
			.parent_names = (const char*[]) {
				"gp2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp3_clk = {
	.halt_reg = 0x0a000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x0a000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_gp3_clk",
			.parent_names = (const char*[]) {
				"gp3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_jpeg_tbu_clk = {
	.halt_reg = 0x12034,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(10),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_jpeg_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};



static struct clk_branch gcc_mss_cfg_ahb_clk = {
	.halt_reg = 0x49000,
	.clkr = {
		.enable_reg = 0x49000,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_mss_cfg_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_mss_q6_bimc_axi_clk = {
	.halt_reg = 0x49004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x49004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_mss_q6_bimc_axi_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm_ahb_clk = {
	.halt_reg = 0x44004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x44004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_pdm_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm2_clk = {
	.halt_reg = 0x4400c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4400c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_pdm2_clk",
			.parent_names = (const char*[]) {
				"pdm2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_prng_ahb_clk = {
	.halt_reg = 0x13004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x45004,
		.enable_mask = BIT(8),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_prng_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_rbcpr_gfx_ahb_clk = {
	.halt_reg = 0x3a008,
	.clkr = {
		.enable_reg = 0x3a008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_rbcpr_gfx_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_rbcpr_gfx_clk = {
	.halt_reg = 0x3a004,
	.clkr = {
		.enable_reg = 0x3a004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_rbcpr_gfx_clk",
			.parent_names = (const char*[]) {
				"rbcpr_gfx_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_ahb_clk = {
	.halt_reg = 0x4201c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4201c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc1_ahb_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_apps_clk = {
	.halt_reg = 0x42018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x42018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc1_apps_clk",
			.parent_names = (const char*[]) {
				"sdcc1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_ice_core_clk = {
	.halt_reg = 0x5d014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5d014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc1_ice_core_clk",
			.parent_names = (const char*[]) {
				"sdcc1_ice_core_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc2_ahb_clk = {
	.halt_reg = 0x4301c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4301c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc2_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc2_apps_clk = {
	.halt_reg = 0x43018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x43018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc2_apps_clk",
			.parent_names = (const char*[]) {
				"sdcc2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc3_ahb_clk = {
	.halt_reg = 0x3901c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3901c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc3_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc3_apps_clk = {
	.halt_reg = 0x39018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x39018,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_sdcc3_apps_clk",
			.parent_names = (const char*[]) {
				"sdcc3_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_smmu_cfg_clk = {
	.halt_reg = 0x12038,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(12),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_smmu_cfg_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb2a_phy_sleep_clk = {
	.halt_reg = 0x4102c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4102c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb2a_phy_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hs_phy_cfg_ahb_clk = {
	.halt_reg = 0x41030,
	.clkr = {
		.enable_reg = 0x41030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb_hs_phy_cfg_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_fs_ahb_clk = {
	.halt_reg = 0x3f008,
	.clkr = {
		.enable_reg = 0x3f008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb_fs_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_fs_ic_clk = {
	.halt_reg = 0x3f030,
	.clkr = {
		.enable_reg = 0x3f030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb_fs_ic_clk",
			.parent_names = (const char*[]) {
				"usb_fs_ic_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hs_ahb_clk = {
	.halt_reg = 0x41008,
	.clkr = {
		.enable_reg = 0x41008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb_hs_ahb_clk",
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hs_system_clk = {
	.halt_reg = 0x41004,
	.clkr = {
		.enable_reg = 0x41004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_usb_hs_system_clk",
			.parent_names = (const char*[]) {
				"usb_hs_system_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus0_ahb_clk = {
	.halt_reg = 0x4c020,
	.clkr = {
		.enable_reg = 0x4c020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_venus0_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus0_axi_clk = {
	.halt_reg = 0x4c024,
	.clkr = {
		.enable_reg = 0x4c024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_venus0_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus0_core0_vcodec0_clk = {
	.halt_reg = 0x4c02c,
	.clkr = {
		.enable_reg = 0x4c02c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_venus0_core0_vcodec0_clk",
			.parent_names = (const char*[]) {
				"vcodec0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus0_core1_vcodec0_clk = {
	.halt_reg = 0x4c034,
	.clkr = {
		.enable_reg = 0x4c034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name ="gcc_venus0_core1_vcodec0_clk",
			.parent_names = (const char*[]) {
				"vcodec0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus0_vcodec0_clk = {
	.halt_reg = 0x4c01c,
	.clkr = {
		.enable_reg = 0x4c01c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_venus0_vcodec0_clk",
			.parent_names = (const char*[]) {
				"vcodec0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus_1_tbu_clk = {
	.halt_reg = 0x1209c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(20),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_venus_1_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_venus_tbu_clk = {
	.halt_reg = 0x12014,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500C,
		.enable_mask = BIT(5),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_venus_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vfe1_tbu_clk = {
	.halt_reg = 0x12090,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500C,
		.enable_mask = BIT(17),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_vfe1_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_vfe_tbu_clk = {
	.halt_reg = 0x1203c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500C,
		.enable_mask = BIT(9),
		.hw.init = &(struct clk_init_data) {
			.name = "gcc_vfe_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_dummy wcnss_m_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data) {
		.name = "wcnss_m_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_regmap *gcc_msm8976_clocks[] = {
	[GPLL0]				= &gpll0.clkr,
	[GPLL2]				= &gpll2.clkr,
	[GPLL3]				= &gpll3.clkr,
	[GPLL4]				= &gpll4.clkr,
	[GPLL6]				= &gpll6.clkr,
	[GPLL0_OUT_MAIN]		= &gpll0_out_main.clkr,
	[GPLL2_OUT_MAIN]		= &gpll2_out_main,
	[GPLL3_OUT_MAIN]		= &gpll3_out_main,
	[GPLL4_OUT_MAIN]		= &gpll4_out_main,
	[GPLL6_OUT_MAIN]		= &gpll6_out_main,
	[GPLL0_AO_OUT_MAIN]		= &gpll0_ao_out_main.clkr,

	[APS_0_CLK_SRC]			= &aps_0_clk_src.clkr,
	[APS_1_CLK_SRC]			= &aps_1_clk_src.clkr,
	[APSS_AHB_CLK_SRC]		= &apss_ahb_clk_src.clkr,
	[BLSP1_QUP1_I2C_APPS_CLK_SRC]	= &blsp1_qup1_i2c_apps_clk_src.clkr,
	[BLSP1_QUP1_SPI_APPS_CLK_SRC]	= &blsp1_qup1_spi_apps_clk_src.clkr,
	[BLSP1_QUP2_I2C_APPS_CLK_SRC]	= &blsp1_qup2_i2c_apps_clk_src.clkr,
	[BLSP1_QUP2_SPI_APPS_CLK_SRC]	= &blsp1_qup2_spi_apps_clk_src.clkr,
	[BLSP1_QUP3_I2C_APPS_CLK_SRC]	= &blsp1_qup3_i2c_apps_clk_src.clkr,
	[BLSP1_QUP3_SPI_APPS_CLK_SRC]	= &blsp1_qup3_spi_apps_clk_src.clkr,
	[BLSP1_QUP4_I2C_APPS_CLK_SRC]	= &blsp1_qup4_i2c_apps_clk_src.clkr,
	[BLSP1_QUP4_SPI_APPS_CLK_SRC]	= &blsp1_qup4_spi_apps_clk_src.clkr,
	[BLSP1_UART1_APPS_CLK_SRC]	= &blsp1_uart1_apps_clk_src.clkr,
	[BLSP1_UART2_APPS_CLK_SRC]	= &blsp1_uart2_apps_clk_src.clkr,
	[BLSP2_QUP1_I2C_APPS_CLK_SRC]	= &blsp2_qup1_i2c_apps_clk_src.clkr,
	[BLSP2_QUP1_SPI_APPS_CLK_SRC]	= &blsp2_qup1_spi_apps_clk_src.clkr,
	[BLSP2_QUP2_I2C_APPS_CLK_SRC]	= &blsp2_qup2_i2c_apps_clk_src.clkr,
	[BLSP2_QUP2_SPI_APPS_CLK_SRC]	= &blsp2_qup2_spi_apps_clk_src.clkr,
	[BLSP2_QUP3_I2C_APPS_CLK_SRC]	= &blsp2_qup3_i2c_apps_clk_src.clkr,
	[BLSP2_QUP3_SPI_APPS_CLK_SRC]	= &blsp2_qup3_spi_apps_clk_src.clkr,
	[BLSP2_QUP4_I2C_APPS_CLK_SRC]	= &blsp2_qup4_i2c_apps_clk_src.clkr,
	[BLSP2_QUP4_SPI_APPS_CLK_SRC]	= &blsp2_qup4_spi_apps_clk_src.clkr,
	[BLSP2_UART1_APPS_CLK_SRC]	= &blsp2_uart1_apps_clk_src.clkr,
	[BLSP2_UART2_APPS_CLK_SRC]	= &blsp2_uart2_apps_clk_src.clkr,
	[CAMSS_GP0_CLK_SRC]		= &camss_gp0_clk_src.clkr,
	[CAMSS_GP1_CLK_SRC]		= &camss_gp1_clk_src.clkr,
	[CAMSS_TOP_AHB_CLK_SRC]		= &camss_top_ahb_clk_src.clkr,
	[CCI_CLK_SRC]			= &cci_clk_src.clkr,
	[CPP_CLK_SRC]			= &cpp_clk_src.clkr,
	[CRYPTO_CLK_SRC]		= &crypto_clk_src.clkr,
	[CSI0_CLK_SRC]			= &csi0_clk_src.clkr,
	[CSI1_CLK_SRC]			= &csi1_clk_src.clkr,
	[CSI2_CLK_SRC]			= &csi2_clk_src.clkr,
	[CSI0PHYTIMER_CLK_SRC]		= &csi0phytimer_clk_src.clkr,
	[CSI1PHYTIMER_CLK_SRC]		= &csi1phytimer_clk_src.clkr,
	[GP1_CLK_SRC]			= &gp1_clk_src.clkr,
	[GP2_CLK_SRC]			= &gp2_clk_src.clkr,
	[GP3_CLK_SRC]			= &gp3_clk_src.clkr,
	[JPEG0_CLK_SRC]			= &jpeg0_clk_src.clkr,
	[MCLK0_CLK_SRC]			= &mclk0_clk_src.clkr,
	[MCLK1_CLK_SRC]			= &mclk1_clk_src.clkr,
	[MCLK2_CLK_SRC]			= &mclk2_clk_src.clkr,
	[PDM2_CLK_SRC]			= &pdm2_clk_src.clkr,
	[RBCPR_GFX_CLK_SRC]		= &rbcpr_gfx_clk_src.clkr,
	[SDCC1_APPS_CLK_SRC]		= &sdcc1_apps_clk_src.clkr,
	[SDCC1_ICE_CORE_CLK_SRC]	= &sdcc1_ice_core_clk_src.clkr,
	[SDCC2_APPS_CLK_SRC]		= &sdcc2_apps_clk_src.clkr,
	[SDCC3_APPS_CLK_SRC]		= &sdcc3_apps_clk_src.clkr,
	[VCODEC0_CLK_SRC]		= &vcodec0_clk_src.clkr,
	[VFE0_CLK_SRC]			= &vfe0_clk_src.clkr,
	[VFE1_CLK_SRC]			= &vfe1_clk_src.clkr,
	[USB_FS_IC_CLK_SRC]		= &usb_fs_ic_clk_src.clkr,
	[USB_FS_SYSTEM_CLK_SRC]		= &usb_fs_system_clk_src.clkr,
	[USB_HS_SYSTEM_CLK_SRC]		= &usb_hs_system_clk_src.clkr,

	[GCC_APS_0_CLK]			= &gcc_aps_0_clk.clkr,
	[GCC_APS_1_CLK]			= &gcc_aps_1_clk.clkr,
	[GCC_APPS_AHB_CLK]		= &gcc_apss_ahb_clk.clkr,
	[GCC_APPS_AXI_CLK]		= &gcc_apss_axi_clk.clkr,
	[GCC_APSS_TCU_CLK]		= &gcc_apss_tcu_clk.clkr,
	[GCC_BLSP1_AHB_CLK]		= &gcc_blsp1_ahb_clk.clkr,
	[GCC_BLSP2_AHB_CLK]		= &gcc_blsp2_ahb_clk.clkr,
	[GCC_BLSP1_QUP1_SPI_APPS_CLK]	= &gcc_blsp1_qup1_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP1_I2C_APPS_CLK]	= &gcc_blsp1_qup1_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP2_I2C_APPS_CLK]	= &gcc_blsp1_qup2_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP2_SPI_APPS_CLK]	= &gcc_blsp1_qup2_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP3_I2C_APPS_CLK]	= &gcc_blsp1_qup3_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP3_SPI_APPS_CLK]	= &gcc_blsp1_qup3_spi_apps_clk.clkr,
	[GCC_BLSP1_QUP4_I2C_APPS_CLK]	= &gcc_blsp1_qup4_i2c_apps_clk.clkr,
	[GCC_BLSP1_QUP4_SPI_APPS_CLK]	= &gcc_blsp1_qup4_spi_apps_clk.clkr,
	[GCC_BLSP1_UART1_APPS_CLK]	= &gcc_blsp1_uart1_apps_clk.clkr,
	[GCC_BLSP1_UART2_APPS_CLK]	= &gcc_blsp1_uart2_apps_clk.clkr,
	[GCC_BLSP2_QUP1_I2C_APPS_CLK]	= &gcc_blsp2_qup1_i2c_apps_clk.clkr,
	[GCC_BLSP2_QUP1_SPI_APPS_CLK]	= &gcc_blsp2_qup1_spi_apps_clk.clkr,
	[GCC_BLSP2_QUP2_I2C_APPS_CLK]	= &gcc_blsp2_qup2_i2c_apps_clk.clkr,
	[GCC_BLSP2_QUP2_SPI_APPS_CLK]	= &gcc_blsp2_qup2_spi_apps_clk.clkr,
	[GCC_BLSP2_QUP3_I2C_APPS_CLK]	= &gcc_blsp2_qup3_i2c_apps_clk.clkr,
	[GCC_BLSP2_QUP3_SPI_APPS_CLK]	= &gcc_blsp2_qup3_spi_apps_clk.clkr,
	[GCC_BLSP2_QUP4_I2C_APPS_CLK]	= &gcc_blsp2_qup4_i2c_apps_clk.clkr,
	[GCC_BLSP2_QUP4_SPI_APPS_CLK]	= &gcc_blsp2_qup4_spi_apps_clk.clkr,
	[GCC_BLSP2_UART1_APPS_CLK]	= &gcc_blsp2_uart1_apps_clk.clkr,
	[GCC_BLSP2_UART2_APPS_CLK]	= &gcc_blsp2_uart2_apps_clk.clkr,
	[GCC_BOOT_ROM_AHB_CLK]		= &gcc_boot_rom_ahb_clk.clkr,
	[GCC_CAMSS_AHB_CLK]		= &gcc_camss_ahb_clk.clkr,
	[GCC_CAMSS_CCI_AHB_CLK]		= &gcc_camss_cci_ahb_clk.clkr,
	[GCC_CAMSS_CCI_CLK]		= &gcc_camss_cci_clk.clkr,
	[GCC_CAMSS_CPP_AHB_CLK]		= &gcc_camss_cpp_ahb_clk.clkr,
	[GCC_CAMSS_CPP_AXI_CLK]		= &gcc_camss_cpp_axi_clk.clkr,
	[GCC_CAMSS_CPP_CLK]		= &gcc_camss_cpp_clk.clkr,
	[GCC_CAMSS_CSI0_AHB_CLK]	= &gcc_camss_csi0_ahb_clk.clkr,
	[GCC_CAMSS_CSI0_CLK]		= &gcc_camss_csi0_clk.clkr,
	[GCC_CAMSS_CSI0PHY_CLK]		= &gcc_camss_csi0phy_clk.clkr,
	[GCC_CAMSS_CSI0PIX_CLK]		= &gcc_camss_csi0pix_clk.clkr,
	[GCC_CAMSS_CSI0RDI_CLK]		= &gcc_camss_csi0rdi_clk.clkr,
	[GCC_CAMSS_CSI1_AHB_CLK]	= &gcc_camss_csi1_ahb_clk.clkr,
	[GCC_CAMSS_CSI1_CLK]		= &gcc_camss_csi1_clk.clkr,
	[GCC_CAMSS_CSI1PHY_CLK]		= &gcc_camss_csi1phy_clk.clkr,
	[GCC_CAMSS_CSI1PIX_CLK]		= &gcc_camss_csi1pix_clk.clkr,
	[GCC_CAMSS_CSI1RDI_CLK]		= &gcc_camss_csi1rdi_clk.clkr,
	[GCC_CAMSS_CSI2_AHB_CLK]	= &gcc_camss_csi2_ahb_clk.clkr,
	[GCC_CAMSS_CSI2_CLK]		= &gcc_camss_csi2_clk.clkr,
	[GCC_CAMSS_CSI2PHY_CLK]		= &gcc_camss_csi2phy_clk.clkr,
	[GCC_CAMSS_CSI2PIX_CLK]		= &gcc_camss_csi2pix_clk.clkr,
	[GCC_CAMSS_CSI2RDI_CLK]		= &gcc_camss_csi2rdi_clk.clkr,
	[GCC_CAMSS_CSI_VFE0_CLK]	= &gcc_camss_csi_vfe0_clk.clkr,
	[GCC_CAMSS_CSI_VFE1_CLK]	= &gcc_camss_csi_vfe1_clk.clkr,
	[GCC_CAMSS_CSI0PHYTIMER_CLK]	= &gcc_camss_csi0phytimer_clk.clkr,
	[GCC_CAMSS_CSI1PHYTIMER_CLK]	= &gcc_camss_csi1phytimer_clk.clkr,
	[GCC_CAMSS_GP0_CLK]		= &gcc_camss_gp0_clk.clkr,
	[GCC_CAMSS_GP1_CLK]		= &gcc_camss_gp1_clk.clkr,
	[GCC_CAMSS_ISPIF_AHB_CLK]	= &gcc_camss_ispif_ahb_clk.clkr,
	[GCC_CAMSS_JPEG0_CLK]		= &gcc_camss_jpeg0_clk.clkr,
	[GCC_CAMSS_JPEG_AHB_CLK]	= &gcc_camss_jpeg_ahb_clk.clkr,
	[GCC_CAMSS_JPEG_AXI_CLK]	= &gcc_camss_jpeg_axi_clk.clkr,
	[GCC_CAMSS_MCLK0_CLK]		= &gcc_camss_mclk0_clk.clkr,
	[GCC_CAMSS_MCLK1_CLK]		= &gcc_camss_mclk1_clk.clkr,
	[GCC_CAMSS_MCLK2_CLK]		= &gcc_camss_mclk2_clk.clkr,
	[GCC_CAMSS_MICRO_AHB_CLK]	= &gcc_camss_micro_ahb_clk.clkr,
	[GCC_CAMSS_TOP_AHB_CLK]		= &gcc_camss_top_ahb_clk.clkr,
	[GCC_CAMSS_VFE0_CLK]		= &gcc_camss_vfe0_clk.clkr,
	[GCC_CAMSS_VFE_AHB_CLK]		= &gcc_camss_vfe_ahb_clk.clkr,
	[GCC_CAMSS_VFE_AXI_CLK]		= &gcc_camss_vfe_axi_clk.clkr,
	[GCC_CAMSS_VFE1_CLK]		= &gcc_camss_vfe1_clk.clkr,
	[GCC_CAMSS_VFE1_AHB_CLK]	= &gcc_camss_vfe1_ahb_clk.clkr,
	[GCC_CAMSS_VFE1_AXI_CLK]	= &gcc_camss_vfe1_axi_clk.clkr,
	[GCC_CPP_TBU_CLK]		= &gcc_cpp_tbu_clk.clkr,
	[GCC_CRYPTO_AHB_CLK]		= &gcc_crypto_ahb_clk.clkr,
	[GCC_CRYPTO_AXI_CLK]		= &gcc_crypto_axi_clk.clkr,
	[GCC_CRYPTO_CLK]		= &gcc_crypto_clk.clkr,
	[GCC_DCC_CLK]			= &gcc_dcc_clk.clkr,
	[GCC_FS_SYSTEM_CLK]		= &gcc_usb_fs_system_clk.clkr,
	[GCC_GP1_CLK]			= &gcc_gp1_clk.clkr,
	[GCC_GP2_CLK]			= &gcc_gp2_clk.clkr,
	[GCC_GP3_CLK]			= &gcc_gp3_clk.clkr,
	[GCC_JPEG_TBU_CLK]		= &gcc_jpeg_tbu_clk.clkr,
	[GCC_MSS_CFG_AHB_CLK]		= &gcc_mss_cfg_ahb_clk.clkr,
	[GCC_MSS_Q6_BIMC_AXI_CLK]	= &gcc_mss_q6_bimc_axi_clk.clkr,
	[GCC_PDM_AHB_CLK]		= &gcc_pdm_ahb_clk.clkr,
	[GCC_PDM2_CLK]			= &gcc_pdm2_clk.clkr,
	[GCC_PRNG_AHB_CLK]		= &gcc_prng_ahb_clk.clkr,
	[GCC_RBCPR_GFX_AHB_CLK]		= &gcc_rbcpr_gfx_ahb_clk.clkr,
	[GCC_RBCPR_GFX_CLK]		= &gcc_rbcpr_gfx_clk.clkr,
	[GCC_SDCC1_AHB_CLK]		= &gcc_sdcc1_ahb_clk.clkr,
	[GCC_SDCC1_APPS_CLK]		= &gcc_sdcc1_apps_clk.clkr,
	[GCC_SDCC1_ICE_CORE_CLK]	= &gcc_sdcc1_ice_core_clk.clkr,
	[GCC_SDCC2_AHB_CLK]		= &gcc_sdcc2_ahb_clk.clkr,
	[GCC_SDCC2_APPS_CLK]		= &gcc_sdcc2_apps_clk.clkr,
	[GCC_SDCC3_AHB_CLK]		= &gcc_sdcc3_ahb_clk.clkr,
	[GCC_SDCC3_APPS_CLK]		= &gcc_sdcc3_apps_clk.clkr,
	[GCC_SMMU_CFG_CLK]		= &gcc_smmu_cfg_clk.clkr,
	[GCC_USB2A_PHY_SLEEP_CLK]	= &gcc_usb2a_phy_sleep_clk.clkr,
	[GCC_USB_HS_PHY_CFG_AHB_CLK]	= &gcc_usb_hs_phy_cfg_ahb_clk.clkr,
	[GCC_USB_FS_AHB_CLK]		= &gcc_usb_fs_ahb_clk.clkr,
	[GCC_USB_FS_IC_CLK]		= &gcc_usb_fs_ic_clk.clkr,
	[GCC_USB_HS_AHB_CLK]		= &gcc_usb_hs_ahb_clk.clkr,
	[GCC_USB_HS_SYSTEM_CLK]		= &gcc_usb_hs_system_clk.clkr,
	[GCC_VENUS0_AHB_CLK]		= &gcc_venus0_ahb_clk.clkr,
	[GCC_VENUS0_AXI_CLK]		= &gcc_venus0_axi_clk.clkr,
	[GCC_VENUS0_CORE0_VCODEC0_CLK]	= &gcc_venus0_core0_vcodec0_clk.clkr,
	[GCC_VENUS0_CORE1_VCODEC0_CLK]	= &gcc_venus0_core1_vcodec0_clk.clkr,
	[GCC_VENUS0_VCODEC0_CLK]	= &gcc_venus0_vcodec0_clk.clkr,
	[GCC_VENUS_1_TBU_CLK]		= &gcc_venus_1_tbu_clk.clkr,
	[GCC_VENUS_TBU_CLK]		= &gcc_venus_tbu_clk.clkr,
	[GCC_VFE1_TBU_CLK]		= &gcc_vfe1_tbu_clk.clkr,
	[GCC_VFE_TBU_CLK]		= &gcc_vfe_tbu_clk.clkr,
};

static const struct qcom_reset_map gcc_msm8976_resets[] = {
	[GCC_CAMSS_MICRO_BCR]		= { 0x56008 },
	[GCC_QUSB2_PHY_BCR]		= { 0x4103c },
	[GCC_USB_HS_BCR]		= { 0x41000 },
	[GCC_USB2_HS_PHY_ONLY_BCR]	= { 0x41034 },
};

static struct clk_hw *gcc_msm8976_hws[] = {
	[GCC_XO]	= &xo.hw,
	[GCC_XO_A]	= &xo_a.hw,
};

static const struct regmap_config gcc_msm8976_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x7fffc,
	.fast_io	= true,
};

static const struct qcom_cc_desc gcc_msm8976_desc = {
	.config = &gcc_msm8976_regmap_config,
	.clks = gcc_msm8976_clocks,
	.num_clks = ARRAY_SIZE(gcc_msm8976_clocks),
	.resets = gcc_msm8976_resets,
	.num_resets = ARRAY_SIZE(gcc_msm8976_resets),
};

static const struct of_device_id gcc_msm8976_match_table[] = {
	{ .compatible = "qcom,gcc-msm8976" },
	{ .compatible = "qcom,gcc-msm8976-v1" },
	{ }
};
MODULE_DEVICE_TABLE(of, gcc_msm8976_match_table);

static int gcc_msm8976_probe(struct platform_device *pdev)
{
	struct clk *clk;
	struct regmap *regmap;
	int i, ret;
	void __iomem *base;
	u32 regval;
	bool compat_bin = false;

	compat_bin = of_device_is_compatible(pdev->dev.of_node,
						 "qcom,gcc-msm8976-v1");

	regmap = qcom_cc_map(pdev, &gcc_msm8976_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (!(PTR_ERR(vdd_dig.regulator[0]) == -EPROBE_DEFER))
			dev_err(&pdev->dev,
					"Unable to get vdd_dig regulator\n");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	if (compat_bin)
		sdcc1_apps_clk_src.freq_tbl = ftbl_sdcc1_v1_apps_clk_src;

	/*Vote for GPLL0 to turn on. Needed by acpuclock. */
	regmap_update_bits(regmap, 0x45000, BIT(0), BIT(0));

	/* Register the dummy measurement clocks */
	for (i = 0; i < ARRAY_SIZE(gcc_msm8976_hws); i++) {
		ret = devm_clk_hw_register(&pdev->dev, gcc_msm8976_hws[i]);
		if (ret)
			return ret;
	}

	clk = devm_clk_register(&pdev->dev, &wcnss_m_clk.hw);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register wcnss_m_clk\n");
			return PTR_ERR(clk);
		}

	ret = qcom_cc_really_probe(pdev, &gcc_msm8976_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GCC clocks\n");
		return ret;
	}

	clk_set_rate(apss_ahb_clk_src.clkr.hw.clk, 19200000);
	clk_prepare_enable(apss_ahb_clk_src.clkr.hw.clk);

	/* Configure Sleep and Wakeup cycles for GMEM clock */
	base = ioremap_nocache(0x1800000, 0x80000);
	regval = readl_relaxed((void __iomem*)(base + 0x59024));
	regval ^= 0xFF0;
	regval |= (0 << 8);
	regval |= (0 << 4);
	writel_relaxed(regval, (void __iomem*)(base + 0x59024));

	clk_pll_configure_sr_hpm_lp(&gpll3, regmap,
					&gpll3_config, true);

	clk_set_rate(gpll3.clkr.hw.clk, 1100000000);

	/* Enable AUX2 clock for APSS */
	regmap_update_bits(regmap, 0x60000, BIT(2), BIT(2));

	dev_info(&pdev->dev, "Registered GCC clocks\n");

	return ret;
}

static struct platform_driver gcc_msm8976_driver = {
	.probe		= gcc_msm8976_probe,
	.driver		= {
		.name	= "gcc-msm8976",
		.of_match_table = gcc_msm8976_match_table,
	},
};

static int __init gcc_msm8976_init(void)
{
	return platform_driver_register(&gcc_msm8976_driver);
}
subsys_initcall(gcc_msm8976_init);

static void __exit gcc_msm8976_exit(void)
{
	platform_driver_unregister(&gcc_msm8976_driver);
}
module_exit(gcc_msm8976_exit);

MODULE_DESCRIPTION("QTI GCC MSM8976 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gcc-msm8976");
