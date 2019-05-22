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
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/pm_opp.h>

#include <dt-bindings/clock/qcom,gpucc-msm8976.h>

#include "common.h"
#include "clk-branch.h"

#include "vdd-level-msm8976.h"

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static DEFINE_VDD_REGS_INIT(vdd_gfx, 1);
static DEFINE_VDD_REGULATORS(vdd_dig, VDD_DIG_NUM, 1, vdd_level);

enum {
	P_GPU_CC_GPLL0,
	P_GPU_CC_GPLL3,
	P_GPU_CC_GPLL4_GFX3D,
	P_GPU_CC_GPLL6_GFX3D,
	P_XO,
};

static const struct parent_map gpu_cc_parent_map_gfx3d[] = {
	{ P_GPU_CC_GPLL0, 1 },
	{ P_GPU_CC_GPLL3, 2 },
	{ P_GPU_CC_GPLL4_GFX3D, 5 },
	{ P_GPU_CC_GPLL6_GFX3D, 3 },
	{ P_XO, 0 },
};

static const char * const gpu_cc_parent_names_gfx3d[] = {
	"gpll0_out_main",
	"gpll3_out_main",
	"gpll4_out_main",
	"gpll6_out_main",
	"xo",
};

static const struct freq_tbl ftbl_gpu_cc_gfx3d_clk_src[] = {
	F(  19200000,                    P_XO,    1,    0,     0),
	F(  50000000,           P_GPU_CC_GPLL0,   16,    0,     0),
	F(  80000000,           P_GPU_CC_GPLL0,   10,    0,     0),
	F( 100000000,           P_GPU_CC_GPLL0,    8,    0,     0),
	F( 133333333,           P_GPU_CC_GPLL0,    6,    0,     0),
	F( 160000000,           P_GPU_CC_GPLL0,    5,    0,     0),
	F( 200000000,           P_GPU_CC_GPLL0,    4,    0,     0),
	F( 228571429,           P_GPU_CC_GPLL0,  3.5,    0,     0),
	F( 240000000,     P_GPU_CC_GPLL6_GFX3D,  4.5,    0,     0),
	F( 266666667,           P_GPU_CC_GPLL0,    3,    0,     0),
	F( 300000000,     P_GPU_CC_GPLL4_GFX3D,    4,    0,     0),
	F( 366670000,           P_GPU_CC_GPLL3,    3,    0,     0),
	F( 400000000,           P_GPU_CC_GPLL0,    2,    0,     0),
	F( 432000000,     P_GPU_CC_GPLL6_GFX3D,  2.5,    0,     0),
	F( 480000000,     P_GPU_CC_GPLL4_GFX3D,  2.5,    0,     0),
	F( 550000000,           P_GPU_CC_GPLL3,    2,    0,     0),
	F( 600000000,     P_GPU_CC_GPLL4_GFX3D,    2,    0,     0),
	{ }
};

static const struct freq_tbl ftbl_gpu_cc_gfx3d_clk_src_v1[] = {
	F(  19200000,                    P_XO,    1,    0,     0),
	F(  50000000,          P_GPU_CC_GPLL0,   16,    0,     0),
	F(  80000000,          P_GPU_CC_GPLL0,   10,    0,     0),
	F( 100000000,          P_GPU_CC_GPLL0,    8,    0,     0),
	F( 133333333,          P_GPU_CC_GPLL0,    6,    0,     0),
	F( 160000000,          P_GPU_CC_GPLL0,    5,    0,     0),
	F( 200000000,          P_GPU_CC_GPLL0,    4,    0,     0),
	F( 228571429,          P_GPU_CC_GPLL0,  3.5,    0,     0),
	F( 240000000,    P_GPU_CC_GPLL6_GFX3D,  4.5,    0,     0),
	F( 266666667,          P_GPU_CC_GPLL0,    3,    0,     0),
	F( 300000000,    P_GPU_CC_GPLL4_GFX3D,    4,    0,     0),
	F( 366670000,          P_GPU_CC_GPLL3,    3,    0,     0),
	F( 400000000,          P_GPU_CC_GPLL0,    2,    0,     0),
	F( 432000000,    P_GPU_CC_GPLL6_GFX3D,  2.5,    0,     0),
	F( 480000000,    P_GPU_CC_GPLL4_GFX3D,  2.5,    0,     0),
	F( 550000000,          P_GPU_CC_GPLL3,    2,    0,     0),
	F( 600000000,    P_GPU_CC_GPLL4_GFX3D,    2,    0,     0),
	//F( 621330000,    gpll2_gfx3d,  1.5,    0,     0),
	{ }
};

static struct clk_rcg2 gpu_cc_gfx3d_clk_src = {
	.cmd_rcgr = 0x59000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = gpu_cc_parent_map_gfx3d,
	.freq_tbl = ftbl_gpu_cc_gfx3d_clk_src,
	.flags = FORCE_ENABLE_RCG,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "gpu_cc_gfx3d_clk_src",
		.parent_names = gpu_cc_parent_names_gfx3d,
		.num_parents = ARRAY_SIZE(gpu_cc_parent_names_gfx3d),
		.ops = &clk_rcg2_ops,
		.vdd_class = &vdd_gfx,
	},
};

static struct clk_branch gpu_cc_oxili_gfx3d_clk = {
	.halt_reg = 0x59020,
	.clkr = {
		.enable_reg = 0x59020,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name ="gpu_cc_oxili_gfx3d_clk",
			.parent_names = (const char*[]) {
				"gpu_cc_gfx3d_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
			VDD_DIG_FMAX_MAP5(LOWER, 300000000, LOW, 366670000,
				NOMINAL, 432000000, NOM_PLUS, 480000000,
				HIGH, 600000000),
		},
	},
};

static struct clk_branch gpu_cc_bimc_gfx_clk = {
	.halt_reg = 0x59048,
	.clkr = {
		.enable_reg = 0x59048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_bimc_gfx_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_oxili_ahb_clk = {
	.halt_reg = 0x59028,
	.clkr = {
		.enable_reg = 0x59028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name ="gpu_cc_oxili_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_oxili_aon_clk = {
	.halt_reg = 0x59044,
	.clkr = {
		.enable_reg = 0x59044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name ="gpu_cc_oxili_aon_clk",
			.parent_names = (const char*[]) {
				"gpu_cc_gfx3d_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_gate2 gpu_cc_oxili_gmem_clk = {
	.udelay = 50,
	.clkr = {
		.enable_reg = 0x59024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name ="gpu_cc_oxili_gmem_clk",
			.parent_names = (const char*[]) {
				"gpu_cc_gfx3d_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_gate2_ops,
		},
	},
};

static struct clk_branch gpu_cc_oxili_timer_clk = {
	.halt_reg = 0x59040,
	.clkr = {
		.enable_reg = 0x59040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data) {
			.name ="gpu_cc_oxili_timer_clk",
			.parent_names = (const char*[]) {
				"xo",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gfx_1_tbu_clk = {
	.halt_reg = 0x12098,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500c,
		.enable_mask = BIT(19),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gfx_1_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gfx_tbu_clk = {
	.halt_reg = 0x12010,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500C,
		.enable_mask = BIT(3),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gfx_tbu_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gfx_tcu_clk = {
	.halt_reg = 0x12020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500C,
		.enable_mask = BIT(2),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gfx_tcu_clk",
			.flags = CLK_ENABLE_HAND_OFF,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gpu_cc_gtcu_ahb_clk = {
	.halt_reg = 0x12044,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x4500C,
		.enable_mask = BIT(13),
		.hw.init = &(struct clk_init_data){
			.name = "gpu_cc_gtcu_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *gpu_cc_msm8976_clocks[] = {
	[GPU_CC_GFX3D_CLK_SRC]		= &gpu_cc_gfx3d_clk_src.clkr,
	[GPU_CC_OXILI_GFX3D_CLK]	= &gpu_cc_oxili_gfx3d_clk.clkr,
	[GPU_CC_BIMC_GFX_CLK]		= &gpu_cc_bimc_gfx_clk.clkr,
	[GPU_CC_OXILI_AHB_CLK]		= &gpu_cc_oxili_ahb_clk.clkr,
	[GPU_CC_OXILI_AON_CLK]		= &gpu_cc_oxili_aon_clk.clkr,
	[GPU_CC_OXILI_GMEM_CLK]		= &gpu_cc_oxili_gmem_clk.clkr,
	[GPU_CC_OXILI_TIMER_CLK]	= &gpu_cc_oxili_timer_clk.clkr,
	[GPU_CC_GFX_1_TBU_CLK]		= &gpu_cc_gfx_1_tbu_clk.clkr,
	[GPU_CC_GFX_TBU_CLK]		= &gpu_cc_gfx_tbu_clk.clkr,
	[GPU_CC_GFX_TCU_CLK]		= &gpu_cc_gfx_tcu_clk.clkr,
	[GPU_CC_GTCU_AHB_CLK]		= &gpu_cc_gtcu_ahb_clk.clkr,
};

static const struct regmap_config gpu_cc_msm8976_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x7fffc,
	.fast_io	= true,
};

static const struct qcom_cc_desc gpu_cc_msm8976_desc = {
	.config = &gpu_cc_msm8976_regmap_config,
	.clks = gpu_cc_msm8976_clocks,
	.num_clks = ARRAY_SIZE(gpu_cc_msm8976_clocks),
};

static const struct of_device_id gpu_cc_msm8976_match_table[] = {
	{ .compatible = "qcom,gpucc-msm8976" },
	{ }
};
MODULE_DEVICE_TABLE(of, gpu_cc_msm8976_e_match_table);

static void get_gfx_version(struct platform_device *pdev, int *version)
{
	struct resource *res;
	void __iomem *base;
	u32 efuse;

	*version = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fuse");
	if (!res) {
		dev_err(&pdev->dev,
			 "No version available. Defaulting to 0.\n");
		return;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base) {
		dev_warn(&pdev->dev,
			 "Unable to read fuse data. Defaulting to 0.\n");
		return;
	}

	efuse = readl_relaxed(base);
	devm_iounmap(&pdev->dev, base);

	*version = (efuse >> 20) & 0x1;

	dev_info(&pdev->dev, "GFX-Version: %d\n", *version);
}

static int of_get_fmax_vdd_class(struct platform_device *pdev,
			struct clk_init_data *clk_intd, char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	int prop_len, i;
	u32 *array;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % 2) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= 2;

	vdd->level_votes = devm_kzalloc(&pdev->dev,
				prop_len * sizeof(*vdd->level_votes),
					GFP_KERNEL);
	if (!vdd->level_votes)
		return -ENOMEM;

	vdd->vdd_uv = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->vdd_uv)
		return -ENOMEM;

	clk_intd->rate_max = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(unsigned long), GFP_KERNEL);
	if (!clk_intd->rate_max)
		return -ENOMEM;

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * 2, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	of_property_read_u32_array(of, prop_name, array, prop_len * 2);
	for (i = 0; i < prop_len; i++) {
		clk_intd->rate_max[i] = array[2 * i];
		vdd->vdd_uv[i] = array[2 * i + 1];
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	clk_intd->num_rate_max = prop_len;

	return 0;
}

static void print_opp_table(struct device *dev, struct clk_hw *hw)
{
	struct dev_pm_opp *opp;
	int i;

	pr_debug("OPP table for GPU core clock:\n");
	for (i = 1; i < hw->init->num_rate_max; i++) {
		if (!hw->init->rate_max[i])
			continue;
		opp = dev_pm_opp_find_freq_exact(dev,
				hw->init->rate_max[i], true);
		pr_info("clock-gpu: OPP voltage for %lu Hz: %ld uV\n",
			hw->init->rate_max[i], dev_pm_opp_get_voltage(opp));
	}
}

/* Find the voltage level required for a given rate. */
int find_vdd_level(struct clk_hw *hw, unsigned long rate)
{
	int level;

	for (level = 0; level < hw->init->num_rate_max; level++)
		if (rate <= hw->init->rate_max[level])
			break;

	if (level == hw->init->num_rate_max) {
		pr_err("Rate %lu for %s is greater than highest Fmax of %lu\n",
			rate, hw->init->name,
			hw->init->rate_max[hw->init->num_rate_max-1]);
		return -EINVAL;
	}

	return level;
}

static void populate_gpu_opp_table(struct platform_device *pdev,
		struct clk_hw *hw)
{
	struct device_node *of = pdev->dev.of_node;
	struct platform_device *gpu_dev;
	struct device_node *gpu_node;
	int i, ret, level;
	unsigned long rate = 0;

	gpu_node = of_parse_phandle(of, "gpu_handle", 0);
	if (!gpu_node) {
		pr_err("clock-gpu: %s: Unable to get device_node pointer for GPU\n",
							__func__);
		return;
	}

	gpu_dev = of_find_device_by_node(gpu_node);
	if (!gpu_dev) {
		pr_err("clock-gpu: %s: Unable to find platform_device node for GPU\n",
							__func__);
		return;
	}

	for (i = 0; i < hw->init->num_rate_max; i++) {
		if (!hw->init->rate_max[i])
			continue;

		ret = clk_round_rate(hw->clk, hw->init->rate_max[i]);
		if (ret < 0) {
			pr_warn("clock-gpu: %s: round_rate failed at %lu - err: %d\n",
							__func__, rate, ret);
			return;
		}
		rate = ret;

		level = find_vdd_level(hw, rate);
		if (level <= 0) {
			pr_warn("no uv for %lu.\n", rate);
			return;
		}

		ret = dev_pm_opp_add(&gpu_dev->dev, rate,
				hw->init->vdd_class->vdd_uv[level]);
		if (ret) {
			pr_warn("clock-gpu: %s: couldn't add OPP for %lu - err: %d\n",
							__func__, rate, ret);
			return;
		}
	}

	print_opp_table(&gpu_dev->dev, hw);
}

static int gpu_cc_msm8976_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct resource *res;
	char prop_name[] = "qcom,gfxfreq-corner-vX";
	void __iomem *base;
	int ret, version;
	u32 val;

	vdd_gfx.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_gfx");
	if (IS_ERR(vdd_gfx.regulator[0])) {
		if (PTR_ERR(vdd_gfx.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_gfx regulator!");
		return PTR_ERR(vdd_gfx.regulator[0]);
	}

	vdd_dig.regulator[0] = devm_regulator_get(&pdev->dev, "vdd_dig");
	if (IS_ERR(vdd_dig.regulator[0])) {
		if (PTR_ERR(vdd_dig.regulator[0]) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get vdd_dig regulator!");
		return PTR_ERR(vdd_dig.regulator[0]);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc_base");
	base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Unable to map GFX3D clock controller.\n");
		return -EINVAL;
	}

	regmap = devm_regmap_init_mmio(dev, base, &gpu_cc_msm8976_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "Unable to map GFX3D MMIO.\n");
		return PTR_ERR(regmap);
	}

	get_gfx_version(pdev, &version);

	snprintf(prop_name, ARRAY_SIZE(prop_name), "qcom,gfxfreq-corner-v%d",
					version);


	ret = of_get_fmax_vdd_class(pdev, (struct clk_init_data *)
				gpu_cc_gfx3d_clk_src.clkr.hw.init, prop_name);
	if (ret) {
		dev_err(&pdev->dev, "Unable to get gfx freq-corner mapping info\n");
		return ret;
	}

	if (version) {
		gpu_cc_gfx3d_clk_src.freq_tbl = ftbl_gpu_cc_gfx3d_clk_src_v1;
		gpu_cc_oxili_gfx3d_clk.clkr.hw.init->rate_max[VDD_DIG_HIGH] = 621330000;
	}

	ret = qcom_cc_really_probe(pdev, &gpu_cc_msm8976_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register GPUCC clocks\n");
		return ret;
	}

	/* Oxili Ocmem in GX rail: OXILI_GMEM_CLAMP_IO */
	regmap_update_bits(regmap, 0x5B00C, BIT(0), 0);

	/* Configure Sleep and Wakeup cycles for OXILI clock */
	val = regmap_read(regmap, 0x59020, &val);
	val &= ~0xF0;
	val |= (0 << 4);
	regmap_write(regmap, 0x59020, val);

	populate_gpu_opp_table(pdev, &gpu_cc_gfx3d_clk_src.clkr.hw);

	dev_info(&pdev->dev, "Registered GPU CC clocks.\n");

	return ret;
}

static struct platform_driver gpu_cc_msm8976_driver = {
	.probe = gpu_cc_msm8976_probe,
	.driver = {
		.name = "gpu_cc-msm8976",
		.of_match_table = gpu_cc_msm8976_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init gpu_cc_msm8976_init(void)
{
	return platform_driver_register(&gpu_cc_msm8976_driver);
}
subsys_initcall(gpu_cc_msm8976_init);

static void __exit gpu_cc_msm8976_exit(void)
{
	platform_driver_unregister(&gpu_cc_msm8976_driver);
}
module_exit(gpu_cc_msm8976_exit);

MODULE_DESCRIPTION("QTI GPU_CC MSM8976 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:gpu_cc-msm8976");
