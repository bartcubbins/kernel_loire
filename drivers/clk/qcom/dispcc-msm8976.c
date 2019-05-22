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

#include <dt-bindings/clock/qcom,dispcc-msm8976.h>

#include "common.h"
#include "clk-branch.h"
#include "clk-voter.h"

#include "vdd-level-msm8976.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_level);

enum {
	P_GPLL6_OUT_MAIN,
	P_GPLL0_OUT_MDP,
	P_DSI0PLL_BYTE,
	P_DSI1PLL_BYTE,
	P_DSI0PLL,
	P_DSI1PLL,
	P_XO,
};

static const struct parent_map gcc_xo_map[] = {
	{ P_XO, 0 },
};

static const char * const gcc_xo[] = {
	"xo",
};

static const struct parent_map gcc_gpll0mdp_gpll6_map[] = {
	{ P_GPLL0_OUT_MDP, 6 },
	{ P_GPLL6_OUT_MAIN, 3 },
};

static const char * const gcc_gpll0mdp_gpll6[] = {
	"gpll0_out_main",
	"gpll6_out_main",
};


static const struct parent_map gcc_parent_map_mdss_byte0[] = {
	{ P_XO, 0 },
	{ P_DSI0PLL_BYTE, 1 },
};

static const char * const gcc_parent_names_mdss_byte0[] = {
	"xo",
	"dsi0pll_byteclk_src",
};

static const struct parent_map gcc_parent_map_mdss_byte1[] = {
	{ P_XO, 0 },
	{ P_DSI0PLL_BYTE, 3 },
	{ P_DSI1PLL_BYTE, 1 },
};

static const struct parent_map gcc_parent_map_mdss_pix0[] = {
	{ P_XO, 0 },
	{ P_DSI0PLL, 1 },
};
static const char * const gcc_parent_names_mdss_pix0[] = {
	"xo",
	"dsi0pll_pclk_src",
};

static const struct parent_map gcc_parent_map_mdss_pix1[] = {
	{ P_XO, 0 },
	{ P_DSI0PLL, 3 },
	{ P_DSI1PLL, 1 },
};

static const char * const gcc_parent_names_mdss_pix1[] = {
	"xo",
	"dsi0pll_pclk_src",
	"dsi1pll_pclk_src",
};

static const char * const gcc_parent_names_mdss_byte1[] = {
	"xo",
	"dsi0pll_byteclk_src",
	"dsi1pll_byteclk_src",
};

static const struct freq_tbl ftbl_esc0_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	{ }
};

static struct clk_rcg2 disp_cc_esc0_clk_src = {
	.cmd_rcgr = 0x4d05c,
	.hid_width = 5,
	.parent_map = gcc_xo_map,
	.freq_tbl = ftbl_esc0_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_esc0_clk_src",
		.parent_names = gcc_xo,
		.num_parents = ARRAY_SIZE(gcc_xo),
		.ops = &clk_esc_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
	},
};

static const struct freq_tbl ftbl_esc1_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	{ }
};

static struct clk_rcg2 disp_cc_esc1_clk_src = {
	.cmd_rcgr = 0x4d0a8,
	.hid_width = 5,
	.parent_map = gcc_xo_map,
	.freq_tbl = ftbl_esc1_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_esc1_clk_src",
		.parent_names = gcc_xo,
		.num_parents = ARRAY_SIZE(gcc_xo),
		.ops = &clk_esc_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
	},
};

static const struct freq_tbl ftbl_mdp_clk_src[] = {
	F(  50000000,           P_GPLL0_OUT_MDP,   16,    0,     0),
	F(  80000000,           P_GPLL0_OUT_MDP,   10,    0,     0),
	F( 100000000,           P_GPLL0_OUT_MDP,    8,    0,     0),
	F( 145454545,           P_GPLL0_OUT_MDP,  5.5,    0,     0),
	F( 160000000,           P_GPLL0_OUT_MDP,    5,    0,     0),
	F( 177777778,           P_GPLL0_OUT_MDP,  4.5,    0,     0),
	F( 200000000,           P_GPLL0_OUT_MDP,    4,    0,     0),
	F( 270000000,          P_GPLL6_OUT_MAIN,    4,    0,     0),
	F( 320000000,           P_GPLL0_OUT_MDP,  2.5,    0,     0),
	F( 360000000,          P_GPLL6_OUT_MAIN,    3,    0,     0),
	{ }
};

static struct clk_rcg2 disp_cc_mdp_clk_src = {
	.cmd_rcgr = 0x4d014,
	.hid_width = 5,
	.parent_map = gcc_gpll0mdp_gpll6_map,
	.freq_tbl = ftbl_mdp_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_mdp_clk_src",
		.parent_names = gcc_gpll0mdp_gpll6,
		.num_parents = ARRAY_SIZE(gcc_gpll0mdp_gpll6),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP4(LOWER, 177780000, LOW, 270000000,
		NOMINAL, 320000000, HIGH, 360000000),
	},
};

static struct clk_rcg2 disp_cc_byte0_clk_src = {
	.cmd_rcgr = 0x4d044,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_mdss_byte0,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_byte0_clk_src",
		.parent_names = gcc_parent_names_mdss_byte0,
		.num_parents = ARRAY_SIZE(gcc_parent_names_mdss_byte0),
		.ops = &clk_byte2_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 125000000, LOW, 161250000,
				NOMINAL, 187500000),
	},
};

static struct clk_rcg2 disp_cc_byte1_clk_src = {
	.cmd_rcgr = 0x4d0b0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gcc_parent_map_mdss_byte1,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_byte1_clk_src",
		.parent_names = gcc_parent_names_mdss_byte1,
		.num_parents = ARRAY_SIZE(gcc_parent_names_mdss_byte1),
		.ops = &clk_byte2_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 125000000, LOW, 161250000,
				NOMINAL, 187500000),
	},
};

static struct clk_rcg2 disp_cc_pclk0_clk_src = {
	.cmd_rcgr = 0x4d000,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_mdss_pix0,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_pclk0_clk_src",
		.parent_names = gcc_parent_names_mdss_pix0,
		.num_parents = ARRAY_SIZE(gcc_parent_names_mdss_pix0),
		.ops = &clk_pixel_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 166670000, LOW, 215000000,
				NOMINAL, 250000000),
	},
};

static struct clk_rcg2 disp_cc_pclk1_clk_src = {
	.cmd_rcgr = 0x4d0b8,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_parent_map_mdss_pix1,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_pclk1_clk_src",
		.parent_names = gcc_parent_names_mdss_pix1,
		.num_parents = ARRAY_SIZE(gcc_parent_names_mdss_pix1),
		.ops = &clk_pixel_ops,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		VDD_DIG_FMAX_MAP3(LOWER, 166670000, LOW, 215000000,
				NOMINAL, 250000000),
	},
};

static const struct freq_tbl ftbl_vsync_clk_src[] = {
	F(  19200000,                      P_XO,    1,    0,     0),
	{ }
};

static struct clk_rcg2 disp_cc_vsync_clk_src = {
	.cmd_rcgr = 0x4d02c,
	.hid_width = 5,
	.parent_map = gcc_xo_map,
	.freq_tbl = ftbl_vsync_clk_src,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "disp_cc_vsync_clk_src",
		.parent_names = gcc_xo,
		.num_parents = ARRAY_SIZE(gcc_xo),
		.ops = &clk_rcg2_ops,
		VDD_DIG_FMAX_MAP1(LOWER, 19200000),
	},
};

static struct clk_branch disp_cc_mdss_esc0_clk = {
	.halt_reg = 0x4d098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4d098,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_esc0_clk",
			.parent_names = (const char*[]) {
				"disp_cc_esc0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc1_clk = {
	.halt_reg = 0x4d09c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4d09c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_esc1_clk",
			.parent_names = (const char*[]) {
				"disp_cc_esc1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdp_tbu_clk = {
	.halt_reg = 0x1201c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(4),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdp_tbu_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdp_rt_tbu_clk = {
	.halt_reg = 0x1204c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(15),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdp_rt_tbu_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_ahb_clk = {
	.halt_reg = 0x4d07c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4d07c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_axi_clk = {
	.halt_reg = 0x4d080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4d080,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_clk = {
	.halt_reg = 0x4D094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4D094,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_byte0_clk",
			.parent_names = (const char*[]) {
				"disp_cc_byte0_clk_src",
			},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE |
				  CLK_SET_RATE_PARENT),
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte1_clk = {
	.halt_reg = 0x4D0A0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4D0A0,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_byte1_clk",
			.parent_names = (const char*[]) {
				"disp_cc_byte1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_clk = {
	.halt_reg = 0x4d088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4d088,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_mdp_clk",
			.parent_names = (const char*[]) {
				"disp_cc_mdp_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk0_clk = {
	.halt_reg = 0x4D084,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4D084,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_pclk0_clk",
			.parent_names = (const char*[]) {
				"disp_cc_pclk0_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk1_clk = {
	.halt_reg = 0x4D0A4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4D0A4,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_pclk1_clk",
			.parent_names = (const char*[]) {
				"disp_cc_pclk1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync_clk = {
	.halt_reg = 0x4d090,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4d090,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name = "disp_cc_mdss_vsync_clk",
			.parent_names = (const char*[]) {
				"disp_cc_vsync_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static DEFINE_CLK_VOTER(disp_cc_mdss_mdp_vote_clk, disp_cc_mdss_mdp_clk, 0);
static DEFINE_CLK_VOTER(disp_cc_mdss_rotator_vote_clk, disp_cc_mdss_mdp_clk, 0);

static struct clk_hw *disp_cc_msm8976_hws[] = {
	[DISP_CC_MDSS_MDP_VOTE_CLK]	= &disp_cc_mdss_mdp_vote_clk.hw,
	[DISP_CC_MDSS_ROTATOR_VOTE_CLK]	= &disp_cc_mdss_rotator_vote_clk.hw,
};

static struct clk_regmap *disp_cc_msm8976_clocks[] = {
	[DISP_CC_ESC0_CLK_SRC]		= &disp_cc_esc0_clk_src.clkr,
	[DISP_CC_ESC1_CLK_SRC]		= &disp_cc_esc1_clk_src.clkr,
	[DISP_CC_MDP_CLK_SRC]		= &disp_cc_mdp_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_CLK_SRC]	= &disp_cc_byte0_clk_src.clkr,
	[DISP_CC_MDSS_BYTE1_CLK_SRC]	= &disp_cc_byte1_clk_src.clkr,
	[DISP_CC_MDSS_PCLK0_CLK_SRC]	= &disp_cc_pclk0_clk_src.clkr,
	[DISP_CC_MDSS_PCLK1_CLK_SRC]	= &disp_cc_pclk1_clk_src.clkr,
	[DISP_CC_VSYNC_CLK_SRC]		= &disp_cc_vsync_clk_src.clkr,

	[DISP_CC_ESC0_CLK]		= &disp_cc_mdss_esc0_clk.clkr,
	[DISP_CC_ESC1_CLK]		= &disp_cc_mdss_esc1_clk.clkr,
	[DISP_CC_MDP_TBU_CLK]		= &disp_cc_mdp_tbu_clk.clkr,
	[DISP_CC_MDP_RT_TBU_CLK]	= &disp_cc_mdp_rt_tbu_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK]		= &disp_cc_mdss_ahb_clk.clkr,
	[DISP_CC_MDSS_AXI_CLK]		= &disp_cc_mdss_axi_clk.clkr,
	[DISP_CC_MDSS_BYTE0_CLK]	= &disp_cc_mdss_byte0_clk.clkr,
	[DISP_CC_MDSS_BYTE1_CLK]	= &disp_cc_mdss_byte1_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK]		= &disp_cc_mdss_mdp_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK]	= &disp_cc_mdss_pclk0_clk.clkr,
	[DISP_CC_MDSS_PCLK1_CLK]	= &disp_cc_mdss_pclk1_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK]	= &disp_cc_mdss_vsync_clk.clkr,
};

static const struct regmap_config disp_cc_msm8976_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x7fffc,
	.fast_io = true,
};

static const struct qcom_cc_desc disp_cc_msm8976_desc = {
	.config = &disp_cc_msm8976_regmap_config,
	.clks = disp_cc_msm8976_clocks,
	.num_clks = ARRAY_SIZE(disp_cc_msm8976_clocks),
	.hwclks = disp_cc_msm8976_hws,
	.num_hwclks = ARRAY_SIZE(disp_cc_msm8976_hws),
};

static const struct of_device_id disp_cc_msm8976_match_table[] = {
	{ .compatible = "qcom,dispcc-msm8976" },
	{ .compatible = "qcom,dispcc-msm8976-v1" },
	{ }
};
MODULE_DEVICE_TABLE(of, disp_cc_msm8976_match_table);

static int disp_cc_msm8976_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct resource *res;
	void __iomem *base;
	int ret;

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (PTR_ERR(vdd_dig.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Unable to map disp_cc controller.\n");
		return -EINVAL;
	}

	regmap = devm_regmap_init_mmio(dev, base, &disp_cc_msm8976_regmap_config);
		if (IS_ERR(regmap)) {
			pr_err("Failed to map the disp_cc registers\n");
			return PTR_ERR(regmap);
		}

	ret = qcom_cc_really_probe(pdev, &disp_cc_msm8976_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register Display CC clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered Display CC clocks\n");

	return ret;
};

static struct platform_driver disp_cc_msm8976_driver = {
	.probe = disp_cc_msm8976_probe,
	.driver = {
		.name = "disp_cc-msm8976",
		.of_match_table = disp_cc_msm8976_match_table,
	},
};

static int __init disp_cc_msm8976_init(void)
{
	return platform_driver_register(&disp_cc_msm8976_driver);
};
subsys_initcall(disp_cc_msm8976_init);

static void __exit disp_cc_msm8976_exit(void)
{
	platform_driver_unregister(&disp_cc_msm8976_driver);
};
module_exit(disp_cc_msm8976_exit);

MODULE_DESCRIPTION("QTI DISP_CC msm8976 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:disp_cc-msm8976");
