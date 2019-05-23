/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/syscore_ops.h>
#include <linux/regulator/rpm-smd-regulator.h>

#include <dt-bindings/clock/qcom,cpu-msm8976.h>

#include "clk-debug.h"
#include "common.h"
#include "clk-pll.h"
#include "clk-hfpll.h"
#include "clk-regmap-mux-div.h"

enum {
	APCS_C0_PLL_BASE,
	APCS_C1_PLL_BASE,
	APCS_CCI_PLL_BASE,
	APCS0_DBG_BASE,
	N_BASES,
};

enum {
	A53SS_MUX_C0,
	A53SS_MUX_C1,
	A53SS_MUX_CCI,
	A53SS_MUX_NUM,
};

enum vdd_mx_pll_levels {
	VDD_MX_OFF,
	VDD_MX_SVS,
	VDD_MX_NOM,
	VDD_MX_TUR,
	VDD_MX_NUM,
};

enum {
	I_CLUSTER_PLL_MAIN,
	I_CLUSTER_PLL,
	P_GPLL0_AO_OUT_MAIN,
	P_GPLL4_OUT_MAIN,
};

static int vdd_hf_levels[] = {
	0,		RPM_REGULATOR_LEVEL_NONE,	/* VDD_PLL_OFF */
	1800000,	RPM_REGULATOR_LEVEL_SVS,	/* VDD_PLL_SVS */
	1800000,	RPM_REGULATOR_LEVEL_NOM,	/* VDD_PLL_NOM */
	1800000,	RPM_REGULATOR_LEVEL_TURBO,	/* VDD_PLL_TUR */
};

static int vdd_sr_levels[] = {
	RPM_REGULATOR_LEVEL_NONE,	/* VDD_PLL_OFF */
	RPM_REGULATOR_LEVEL_SVS,	/* VDD_PLL_SVS */
	RPM_REGULATOR_LEVEL_NOM,	/* VDD_PLL_NOM */
	RPM_REGULATOR_LEVEL_TURBO,	/* VDD_PLL_TUR */
};

static DEFINE_VDD_REGS_INIT(vdd_cpu_a72, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_a53, 1);
static DEFINE_VDD_REGS_INIT(vdd_cpu_cci, 1);

static DEFINE_VDD_REGULATORS(vdd_hf, VDD_MX_NUM, 2,
				vdd_hf_levels);

static DEFINE_VDD_REGULATORS(vdd_mx_sr, VDD_MX_NUM, 1,
				vdd_sr_levels);

#define CPU_LATENCY_NO_L2_PC_US (280)

#define to_clk_regmap_mux_div(_hw) \
	container_of(to_clk_regmap(_hw), struct clk_regmap_mux_div, clkr)

#define VDD_MX_HF_FMAX_MAP2(l1, f1, l2, f2)		\
	.vdd_class = &vdd_hf,				\
	.rate_max = (unsigned long[VDD_MX_NUM]) {	\
		[VDD_MX_##l1] = (f1),			\
		[VDD_MX_##l2] = (f2),			\
	},						\
	.num_rate_max = VDD_MX_NUM

#define VDD_MX_SR_FMAX_MAP2(l1, f1, l2, f2)		\
	.vdd_class = &vdd_mx_sr,			\
	.rate_max = (unsigned long[VDD_MX_NUM]) {	\
		[VDD_MX_##l1] = (f1),			\
		[VDD_MX_##l2] = (f2),			\
	},						\
	.num_rate_max = VDD_MX_NUM

static const struct parent_map cpu_cc_parent_map_a53[] = {
	{ I_CLUSTER_PLL_MAIN, 3 },
	{ I_CLUSTER_PLL, 5},
	{ P_GPLL0_AO_OUT_MAIN, 4 },
	{ P_GPLL4_OUT_MAIN, 1 },
};

static const char * const cpu_cc_parent_names_a53[] = {
	"a53ss_sr_pll_main",
	"a53ss_sr_pll",
	"gpll0_ao_out_main",
	"gpll4_out_main",
};

static const struct parent_map cpu_cc_parent_map_a72[] = {
	{ I_CLUSTER_PLL_MAIN, 3 },
	{ I_CLUSTER_PLL, 5},
	{ P_GPLL0_AO_OUT_MAIN, 4 },
	{ P_GPLL4_OUT_MAIN, 1 },
};

static const char * const cpu_cc_parent_names_a72[] = {
	"a72ss_hf_pll_main",
	"a72ss_hf_pll",
	"gpll0_ao_out_main",
	"gpll4_out_main",
};

static const struct parent_map cpu_cc_parent_map_cci[] = {
	{ I_CLUSTER_PLL_MAIN, 3 },
	{ I_CLUSTER_PLL, 5},
	{ P_GPLL0_AO_OUT_MAIN, 4 },
};

static const char * const cpu_cc_parent_names_cci[] = {
	"cci_sr_pll_main",
	"cci_sr_pll",
	"gpll0_ao_out_main",
};

/* Early output of A53 PLL */
static struct hfpll_data a53ss_sr_pll_data = {
	.mode_reg = 0x0,
	.l_reg = 0x4,
	.m_reg = 0x8,
	.n_reg = 0xC,
	.user_reg = 0x10,
	.config_reg = 0x14,
	.status_reg = 0x1C,
	.lock_bit = 16,
	.spm_offset = 0x50,
	.spm_event_bit = 0x4,
	.user_vco_mask = 0x3 << 20,
	.pre_div_mask = 0x7 << 12,
	.post_div_mask = (BIT(8) | BIT(9)),
	.post_div_masked =  0x1 << 8,
	.early_output_mask =  BIT(3),
	.main_output_mask = BIT(0),
	.config_val = 0x00341600,
	.user_vco_val = 0x00141400,
	.vco_mode_masked = BIT(20),
	.min_rate = 652800000UL,
	.max_rate = 1478400000UL,
	.low_vco_max_rate = 902400000UL,
	.l_val = 0x49,
};

/* Early output of A72 PLL */
static struct hfpll_data a72ss_hf_pll_data = {
	.mode_reg = 0x0,
	.l_reg = 0x4,
	.m_reg = 0x8,
	.n_reg = 0xC,
	.user_reg = 0x10,
	.config_reg = 0x14,
	.status_reg = 0x1C,
	.lock_bit = 16,
	.spm_offset = 0x50,
	.spm_event_bit = 0x4,
	.user_vco_mask = 0x3 << 28,
	.pre_div_mask = BIT(12),
	.pre_div_masked = 0,
	.post_div_mask = (BIT(8) | BIT(9)),
	.post_div_masked = 0x100,
	.early_output_mask =  0x8,
	.main_output_mask = BIT(0),
	.vco_mode_masked = BIT(20),
	.config_val = 0x04E0405D,
	.max_rate = 2016000000UL,
	.min_rate = 940800000UL,
	.l_val = 0x5B,
};

/* Early output of CCI PLL */
static struct hfpll_data cci_sr_pll_data = {
	.mode_reg = 0x0,
	.l_reg = 0x4,
	.m_reg = 0x8,
	.n_reg = 0xC,
	.user_reg = 0x10,
	.config_reg = 0x14,
	.status_reg = 0x1C,
	.lock_bit = 16,
	.spm_offset = 0x40,
	.spm_event_bit = 0x0,
	.user_vco_mask = 0x3 << 20,
	.pre_div_mask = 0x7 << 12,
	.post_div_mask = (BIT(8) | BIT(9)),
	.early_output_mask =  BIT(3),
	.main_output_mask = BIT(0),
	.post_div_masked = 0x1 << 8,
	.vco_mode_masked = BIT(20),
	.config_val = 0x00141400,
	.min_rate = 307200000UL,
	.max_rate = 902400000UL,
	.l_val = 0x20,
};

static struct clk_hfpll a53ss_sr_pll = {
	.d = &a53ss_sr_pll_data,
	.init_done = false,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "a53ss_sr_pll",
		.parent_names = (const char *[]) { "xo_a" },
		.num_parents = 1,
		.ops = &clk_ops_hf2_pll,
		VDD_MX_SR_FMAX_MAP2(SVS, 1000000000, NOM, 2200000000UL),
	},
};

static struct clk_hfpll a72ss_hf_pll = {
	.d = &a72ss_hf_pll_data,
	.init_done = false,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "a72ss_hf_pll",
		.parent_names = (const char *[]) { "xo_a" },
		.num_parents = 1,
		.ops = &clk_ops_hf2_pll,
		/* MX level of MSM is much higher than of PLL */
		VDD_MX_HF_FMAX_MAP2(SVS, 2000000000, NOM, 2900000000UL),
	},
};

static struct clk_hfpll cci_sr_pll = {
	.d = &cci_sr_pll_data,
	.init_done = false,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "cci_sr_pll",
		.parent_names = (const char *[]) { "xo_a" },
		.num_parents = 1,
		.ops = &clk_ops_hf2_pll,
		VDD_MX_SR_FMAX_MAP2(SVS, 1000000000, NOM, 2200000000UL),
	},
};

static struct clk_fixed_factor a53ss_sr_pll_main = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "a53ss_sr_pll_main",
		.parent_names = (const char *[]) { "a53ss_sr_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor a72ss_hf_pll_main = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "a72ss_hf_pll_main",
		.parent_names = (const char *[]) { "a72ss_hf_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor cci_sr_pll_main = {
	.mult = 1,
	.div = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "cci_sr_pll_main",
		.parent_names = (const char *[]) { "cci_sr_pll" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor sys_apcsaux_clk_2 = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "sys_apcsaux_clk_2",
		.parent_names = (const char*[]) { "gpll4_out_main" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor sys_apcsaux_clk_3 = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data) {
		.name = "sys_apcsaux_clk_3",
		.parent_names = (const char*[]) { "gpll0_ao_out_main" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_hfpll *cpu_ccpll[] = {
	&a53ss_sr_pll,
	&a72ss_hf_pll,
	&cci_sr_pll
};

static struct hfpll_data *cpu_ccpll_data[] = {
	&a53ss_sr_pll_data,
	&a72ss_hf_pll_data,
	&cci_sr_pll_data
};

static const char const *mux_names[] = { "c0", "c1", "cci" };

static const struct regmap_config cpu_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x34,
	.fast_io = true,
};

static unsigned long cpu_cc_recalc_rate(struct clk_hw *hw,
			unsigned long prate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	struct clk_hw *parent;
	const char *name = clk_hw_get_name(hw);
	unsigned long parent_rate;
	u32 i, div, src = 0;
	u32 num_parents = clk_hw_get_num_parents(hw);
	int ret = 0;

	ret = mux_div_get_src_div(cpuclk, &src, &div);
	if (ret)
		return ret;

	cpuclk->src = src;
	cpuclk->div = div;

	for (i = 0; i < num_parents; i++) {
		if (src == cpuclk->parent_map[i].cfg) {
			parent = clk_hw_get_parent_by_index(hw, i);
			parent_rate = clk_hw_get_rate(parent);
			return clk_rcg2_calc_rate(parent_rate, 0, 0, 0, div);
		}
	}
	pr_err("%s: Can't find parent %d\n", name, src);

	return ret;
}

struct cpu_clk_msm8976 {
	cpumask_t cpumask;
	struct pm_qos_request req;
};

enum {
	AUX_DIV2,
	AUX_FULL,
	AUX_MAX_AVAILABLE_RATES,
};

struct cpu_cc_8976_data {
	struct cpu_clk_msm8976 a53ssmux_desc;
	struct cpu_clk_msm8976 a72ssmux_desc;
	unsigned long aux3_rates[AUX_MAX_AVAILABLE_RATES];
	unsigned long aux2_rates[AUX_MAX_AVAILABLE_RATES];
};
static struct cpu_cc_8976_data cpu_data;

static void do_nothing(void *unused) { }

static int cpu_cc_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	return __mux_div_set_src_div(cpuclk, cpuclk->src, cpuclk->div);
}

static int cpu_cc_set_rate_and_parent(struct clk_hw *hw,
						unsigned long rate,
						unsigned long prate,
						u8 index)
{
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);

	return __mux_div_set_src_div(cpuclk, cpuclk->parent_map[index].cfg,
					cpuclk->div);
}

static void cpu_cc_pm_qos_add_req(struct cpu_clk_msm8976* cluster_desc)
{
	memset(&cluster_desc->req, 0, sizeof(cluster_desc->req));
	cpumask_copy(&(cluster_desc->req.cpus_affine),
			(const struct cpumask *)&cluster_desc->cpumask);
	cluster_desc->req.type = PM_QOS_REQ_AFFINE_CORES;
	pm_qos_add_request(&cluster_desc->req, PM_QOS_CPU_DMA_LATENCY,
			CPU_LATENCY_NO_L2_PC_US);
	smp_call_function_any(&cluster_desc->cpumask, do_nothing,
				NULL, 1);
}

static int cpu_cc_set_rate_a53(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	int rc;

	cpu_cc_pm_qos_add_req(&cpu_data.a53ssmux_desc);

	rc = cpu_cc_set_rate(hw, rate, parent_rate);

	pm_qos_remove_request(&cpu_data.a53ssmux_desc.req);

	return rc;
}

static int cpu_cc_set_rate_and_parent_a53(struct clk_hw *hw,
			unsigned long rate, unsigned long prate, u8 index)
{
	int rc;

	cpu_cc_pm_qos_add_req(&cpu_data.a53ssmux_desc);

	rc = cpu_cc_set_rate_and_parent(hw, rate, prate, index);

	pm_qos_remove_request(&cpu_data.a53ssmux_desc.req);

	return rc;
}


static int cpu_cc_set_rate_a72(struct clk_hw *hw,
			unsigned long rate, unsigned long parent_rate)
{
	int rc;

	cpu_cc_pm_qos_add_req(&cpu_data.a72ssmux_desc);

	rc = cpu_cc_set_rate(hw, rate, parent_rate);

	pm_qos_remove_request(&cpu_data.a72ssmux_desc.req);

	return rc;
}

static int cpu_cc_set_rate_and_parent_a72(struct clk_hw *hw,
			unsigned long rate, unsigned long prate, u8 index)
{
	int rc;

	cpu_cc_pm_qos_add_req(&cpu_data.a72ssmux_desc);

	rc = cpu_cc_set_rate_and_parent(hw, rate, prate, index);

	pm_qos_remove_request(&cpu_data.a72ssmux_desc.req);

	return rc;
}

static int cpu_cc_enable(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.enable(hw);
}

static void cpu_cc_disable(struct clk_hw *hw)
{
	clk_regmap_mux_div_ops.disable(hw);
}

static u8 cpu_cc_get_parent(struct clk_hw *hw)
{
	return clk_regmap_mux_div_ops.get_parent(hw);
}

static int cpu_cc_set_parent(struct clk_hw *hw, u8 index)
{
	/*
	 * Since cpucc_clk_set_rate_and_parent() is defined and set_parent()
	 * will never gets called from clk_change_rate() so return 0.
	 */
	return 0;
}

static int cpu_cc_determine_rate(struct clk_hw *hw,
			struct clk_rate_request *req)
{
	int ret;
	u32 div = 1;
	struct clk_hw *clk_parent, *cpu_pll_hw;
	unsigned long mask, pll_rate, rate = req->rate;
	struct clk_rate_request parent_req = { };
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	int clk_index = P_GPLL0_AO_OUT_MAIN; /* Default to GPLL0_AO auxiliary */

	cpu_pll_hw = clk_hw_get_parent_by_index(hw, I_CLUSTER_PLL);
	if (!cpu_pll_hw) {
		/* Force using the APCS safe auxiliary source (GPLL0_AO) */
		pll_rate = ULONG_MAX;
	} else {
		pll_rate = cpu_data.aux3_rates[AUX_FULL];

		/* If GPLL0_AO is out of range, try to use GPLL4 */
		if (rate > cpu_data.aux3_rates[AUX_DIV2] &&
		    rate != pll_rate) {
			clk_index = P_GPLL4_OUT_MAIN;
			clk_parent =
				clk_hw_get_parent_by_index(hw, P_GPLL4_OUT_MAIN);
			pll_rate = cpu_data.aux2_rates[AUX_FULL];
		}
	}

	if (rate <= pll_rate) {
		/* Use one of the APCSAUX as clock source */
		clk_parent = clk_hw_get_parent_by_index(hw, clk_index);
		mask = BIT(cpuclk->hid_width) - 1;

		/*
		 * Avoid powering on the specific cluster PLL to save
		 * power whenever a low CPU frequency is requested for
		 * that cluster.
		 */
		req->best_parent_hw = clk_parent;
		req->best_parent_rate = pll_rate;

		div = DIV_ROUND_UP((2 * req->best_parent_rate), rate) - 1;
		div = min_t(unsigned long, div, mask);

		req->rate = req->best_parent_rate * 2;
		req->rate /= div + 1;

		cpuclk->src = cpuclk->parent_map[clk_index].cfg;
	} else {
		/* Use the cluster specific PLL as clock source */
		clk_index = I_CLUSTER_PLL_MAIN;
		clk_parent = clk_hw_get_parent_by_index(hw, clk_index);

		/*
		 * Originally, we would run the PLL _always_ at maximum
		 * frequency and postdivide the frequency to get where
		 * we want to be, but we can save some battery time.
		 *
		 * To save power, it's better to set the PLL to give us
		 * the clock that we want.
		 */
		parent_req.rate = rate;
		parent_req.best_parent_hw = clk_parent;

		req->best_parent_hw = clk_parent;
		ret = __clk_determine_rate(req->best_parent_hw, &parent_req);

		cpuclk->src = cpuclk->parent_map[I_CLUSTER_PLL].cfg;
		req->best_parent_rate = parent_req.rate;
	}
	cpuclk->div = div;

	return 0;
}

static int cpu_cc_determine_rate_cci(struct clk_hw *hw,
			struct clk_rate_request *req)
{
	int ret;
	u32 div = 1;
	struct clk_hw *cpu_pll_main_hw;
	unsigned long rate = req->rate;
	struct clk_rate_request parent_req = { };
	struct clk_regmap_mux_div *cpuclk = to_clk_regmap_mux_div(hw);
	int pll_clk_index = I_CLUSTER_PLL;

	cpu_pll_main_hw = clk_hw_get_parent_by_index(hw, I_CLUSTER_PLL_MAIN);

	parent_req.rate = rate;
	parent_req.best_parent_hw = cpu_pll_main_hw;

	req->best_parent_hw = cpu_pll_main_hw;
	ret = __clk_determine_rate(req->best_parent_hw, &parent_req);

	cpuclk->src = cpuclk->parent_map[pll_clk_index].cfg;
	req->best_parent_rate = parent_req.rate;

	cpuclk->div = div;

	return 0;
}

static const struct clk_ops cpu_cc_clk_ops_a53 = {
	.enable = cpu_cc_enable,
	.disable = cpu_cc_disable,
	.get_parent = cpu_cc_get_parent,
	.set_rate = cpu_cc_set_rate_a53,
	.set_rate_and_parent = cpu_cc_set_rate_and_parent_a53,
	.set_parent = cpu_cc_set_parent,
	.recalc_rate = cpu_cc_recalc_rate,
	.determine_rate = cpu_cc_determine_rate,
	.debug_init = clk_debug_measure_add,
};

static const struct clk_ops cpu_cc_clk_ops_a72 = {
	.enable = cpu_cc_enable,
	.disable = cpu_cc_disable,
	.get_parent = cpu_cc_get_parent,
	.set_rate = cpu_cc_set_rate_a72,
	.set_rate_and_parent = cpu_cc_set_rate_and_parent_a72,
	.set_parent = cpu_cc_set_parent,
	.recalc_rate = cpu_cc_recalc_rate,
	.determine_rate = cpu_cc_determine_rate,
	.debug_init = clk_debug_measure_add,
};

static const struct clk_ops cpu_cc_clk_ops_cci = {
	.enable = cpu_cc_enable,
	.disable = cpu_cc_disable,
	.get_parent = cpu_cc_get_parent,
	.set_rate = cpu_cc_set_rate,
	.set_rate_and_parent = cpu_cc_set_rate_and_parent,
	.set_parent = cpu_cc_set_parent,
	.recalc_rate = cpu_cc_recalc_rate,
	.determine_rate = cpu_cc_determine_rate_cci,
	.debug_init = clk_debug_measure_add,
};

static int cpu_cc_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct clk_regmap_mux_div *cpuclk = container_of(nb,
					struct clk_regmap_mux_div, clk_nb);
	int ret = 0;
	int safe_src = cpuclk->safe_src;

	if (event == PRE_RATE_CHANGE)
		/* set the mux to safe source(sys_apc0_aux_clk) & div */
		ret = __mux_div_set_src_div(cpuclk, safe_src, 1);

	if (event == ABORT_RATE_CHANGE)
		pr_err("Error in configuring PLL - stay at safe src only\n");

	return notifier_from_errno(ret);
}

static int cpu_cc_cci_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct clk_regmap_mux_div *cpuclk = container_of(nb,
					struct clk_regmap_mux_div, clk_nb);
	int ret = 0;
	int safe_src = cpuclk->safe_src;

	if (event == PRE_RATE_CHANGE)
		/* set the mux to safe source(sys_apc0_aux_clk) & div */
		ret = __mux_div_set_src_div(cpuclk, safe_src, 4);

	if (event == ABORT_RATE_CHANGE)
		pr_err("Error in configuring PLL - stay at safe src only\n");

	return notifier_from_errno(ret);
}

static struct clk_regmap_mux_div a53ssmux = {
	.reg_offset = 0x0,
	.hid_width = 5,
	.hid_shift = 0,
	.src_width = 3,
	.src_shift = 8,
	.safe_src = 4,
	.safe_div = 2,
	.safe_freq = 400000000,
	.parent_map = cpu_cc_parent_map_a53,
	.clk_nb.notifier_call = cpu_cc_notifier_cb,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "a53ssmux",
		.parent_names = cpu_cc_parent_names_a53,
		.num_parents = ARRAY_SIZE(cpu_cc_parent_names_a53),
		.ops = &cpu_cc_clk_ops_a53,
		.vdd_class = &vdd_cpu_a53,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap_mux_div a72ssmux = {
	.reg_offset = 0x0,
	.hid_width = 5,
	.hid_shift = 0,
	.src_width = 3,
	.src_shift = 8,
	.safe_src = 4,
	.safe_div = 2,
	.safe_freq = 400000000,
	.parent_map = cpu_cc_parent_map_a72,
	.clk_nb.notifier_call = cpu_cc_notifier_cb,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "a72ssmux",
		.parent_names = cpu_cc_parent_names_a72,
		.num_parents = ARRAY_SIZE(cpu_cc_parent_names_a72),
		.ops = &cpu_cc_clk_ops_a72,
		.vdd_class = &vdd_cpu_a72,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap_mux_div ccissmux = {
	.reg_offset = 0x0,
	.hid_width = 5,
	.hid_shift = 0,
	.src_width = 3,
	.src_shift = 8,
	.safe_src = 4,
	.safe_div = 4,
	.safe_freq = 200000000,
	.parent_map = cpu_cc_parent_map_cci,
	.clk_nb.notifier_call = cpu_cc_cci_notifier_cb,
	.clkr.hw.init = &(struct clk_init_data) {
		.name = "ccissmux",
		.parent_names = cpu_cc_parent_names_cci,
		.num_parents = ARRAY_SIZE(cpu_cc_parent_names_cci),
		.ops = &cpu_cc_clk_ops_cci,
		.vdd_class = &vdd_cpu_cci,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap_mux_div *cpu_cc_mux[] = {
	&a53ssmux,
	&a72ssmux,
	&ccissmux
};

static struct clk_hw *cpu_clks_hws[] = {
	/* PLL Sources */
	[SYS_APCSAUX_CLK_2]	= &sys_apcsaux_clk_2.hw,
	[SYS_APCSAUX_CLK_3]	= &sys_apcsaux_clk_3.hw,
	/* PLL */
	[A53SS_SR_PLL]		= &a53ss_sr_pll.clkr.hw,
	[A72SS_HF_PLL]		= &a72ss_hf_pll.clkr.hw,
	[CCI_SR_PLL]		= &cci_sr_pll.clkr.hw,
	[A53SS_SR_PLL_MAIN]	= &a53ss_sr_pll_main.hw,
	[A72SS_HF_PLL_MAIN]	= &a72ss_hf_pll_main.hw,
	[CCI_SR_PLL_MAIN]	= &cci_sr_pll_main.hw,
	/* Muxes */
	[A53SSMUX]		= &a53ssmux.clkr.hw,
	[A72SSMUX]		= &a72ssmux.clkr.hw,
	[CCISSMUX]		= &ccissmux.clkr.hw,
};

/*
 *  Find the voltage level required for a given clock rate.
 */
static int cpu_cc_find_vdd_level(struct clk_init_data *clk_intd,
			unsigned long rate)
{
	int level;

	for (level = 0; level < clk_intd->num_rate_max; level++)
		if (rate <= clk_intd->rate_max[level])
			break;

	if (level == clk_intd->num_rate_max) {
		pr_err("Rate %lu for %s is greater than highest Fmax\n", rate,
				clk_intd->name);
		return -EINVAL;
	}

	return level;
}

static int cpu_cc_add_opp(struct clk_hw *hw, struct device *dev,
			unsigned long max_rate)
{
	struct clk_init_data *clk_intd = (struct clk_init_data *)hw->init;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	unsigned long rate = 0;
	long ret;
	int level, uv, j = 1;

	if (IS_ERR_OR_NULL(dev)) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	while (1) {
		rate = clk_intd->rate_max[j++];
		level = cpu_cc_find_vdd_level(clk_intd, rate);
		if (level <= 0) {
			pr_warn("clock-cpu: no corner for %lu.\n", rate);
			return -EINVAL;
		}

		uv = vdd->vdd_uv[level];
		if (uv < 0) {
			pr_warn("clock-cpu: no uv for %lu.\n", rate);
			return -EINVAL;
		}

		ret = dev_pm_opp_add(dev, rate, uv);
		if (ret) {
			pr_warn("clock-cpu: failed to add OPP for %lu\n", rate);
			return rate;
		}

		if (rate >= max_rate)
			break;
	}

	return 0;
}

static void cpu_cc_print_opp_table(int a53_cpu, int a72_cpu)
{
	struct dev_pm_opp *oppfmax, *oppfmin;
	unsigned long apc0_fmax, apc1_fmax;
	unsigned long apc0_fmin, apc1_fmin;
	u32 apc0_max_index, apc1_max_index;

	apc0_max_index = a53ssmux.clkr.hw.init->num_rate_max;
	apc1_max_index = a72ssmux.clkr.hw.init->num_rate_max;
	apc0_fmax = a53ssmux.clkr.hw.init->rate_max[apc0_max_index - 1];
	apc1_fmax = a72ssmux.clkr.hw.init->rate_max[apc1_max_index - 1];
	apc0_fmin = a53ssmux.clkr.hw.init->rate_max[1];
	apc1_fmin = a72ssmux.clkr.hw.init->rate_max[1];

	/*
	 * One time information during boot. Important to know that this looks
	 * sane since it can eventually make its way to the scheduler.
	 */
	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a53_cpu),
					apc0_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a53_cpu),
					apc0_fmin, true);
	pr_info("clock_cpu: a53: OPP voltage for %lu: %lu\n", apc0_fmin,
					dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a53: OPP voltage for %lu: %lu\n", apc0_fmax,
					dev_pm_opp_get_voltage(oppfmax));

	oppfmax = dev_pm_opp_find_freq_exact(get_cpu_device(a72_cpu),
					apc1_fmax, true);
	oppfmin = dev_pm_opp_find_freq_exact(get_cpu_device(a72_cpu),
					apc1_fmin, true);
	pr_info("clock_cpu: a72: OPP voltage for %lu: %lu\n", apc1_fmin,
					dev_pm_opp_get_voltage(oppfmin));
	pr_info("clock_cpu: a72: OPP voltage for %lu: %lu\n", apc1_fmax,
					dev_pm_opp_get_voltage(oppfmax));
}

static void cpu_cc_populate_opp_table(struct platform_device *pdev)
{
	struct platform_device *apc0_dev, *apc1_dev;
	struct device_node *apc0_node, *apc1_node;
	unsigned long apc0_fmax, apc1_fmax;
	int cpu, a53_cpu = 0, a72_cpu = 0;
	u32 apc0_max_index, apc1_max_index;

	apc0_node = of_parse_phandle(pdev->dev.of_node, "vdd_a53-supply", 0);
	if (!apc0_node) {
		pr_err("can't find the apc0 dt node.\n");
		return;
	}

	apc1_node = of_parse_phandle(pdev->dev.of_node, "vdd_a72-supply", 0);
	if (!apc1_node) {
		pr_err("can't find the apc1 dt node.\n");
		return;
	}

	apc0_dev = of_find_device_by_node(apc0_node);
	if (!apc0_dev) {
		pr_err("can't find the apc0 device node.\n");
		return;
	}

	apc1_dev = of_find_device_by_node(apc1_node);
	if (!apc1_dev) {
		pr_err("can't find the apc1 device node.\n");
		return;
	}

	apc0_max_index = a53ssmux.clkr.hw.init->num_rate_max;
	apc1_max_index = a72ssmux.clkr.hw.init->num_rate_max;
	apc0_fmax = a53ssmux.clkr.hw.init->rate_max[apc0_max_index - 1];
	apc1_fmax = a72ssmux.clkr.hw.init->rate_max[apc1_max_index - 1];

	for_each_possible_cpu(cpu) {
		if (cpu <= 3) {
			a53_cpu = cpu;
			WARN(cpu_cc_add_opp(&a53ssmux.clkr.hw, get_cpu_device(cpu),
				     apc0_fmax),
				     "Failed to add OPP levels for A53\n");
		} else {
			a72_cpu = cpu;
			WARN(cpu_cc_add_opp(&a72ssmux.clkr.hw, get_cpu_device(cpu),
				     apc1_fmax),
				     "Failed to add OPP levels for A72\n");
		}
	}

	/* One time print during bootup */
	pr_info("clock-cpu-8976: OPP tables populated (cpu %d and %d)\n",
							a53_cpu, a72_cpu);

	cpu_cc_print_opp_table(a53_cpu, a72_cpu);
}

static int cpu_cc_get_fmax_vdd_class(struct platform_device *pdev,
			struct clk_init_data *clk_intd, char *prop_name)
{
	struct device_node *of = pdev->dev.of_node;
	struct clk_vdd_class *vdd = clk_intd->vdd_class;
	int prop_len, i, j, ret;
	int num = vdd->num_regulators + 1;
	u32 *array;

	if (!of_find_property(of, prop_name, &prop_len)) {
		dev_err(&pdev->dev, "missing %s\n", prop_name);
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % num) {
		dev_err(&pdev->dev, "bad length %d\n", prop_len);
		return -EINVAL;
	}

	prop_len /= num;

	vdd->level_votes = devm_kzalloc(&pdev->dev, prop_len * sizeof(int),
					GFP_KERNEL);
	if (!vdd->level_votes) {
		pr_err("Cannot allocate memory for level_votes\n");
		return -ENOMEM;
	}

	vdd->vdd_uv = devm_kzalloc(&pdev->dev,
		prop_len * sizeof(int) * (num - 1), GFP_KERNEL);
	if (!vdd->vdd_uv) {
		pr_err("Cannot allocate memory for vdd_uv\n");
		return -ENOMEM;
	}

	clk_intd->rate_max = devm_kzalloc(&pdev->dev,
					prop_len * sizeof(unsigned long),
					GFP_KERNEL);
	if (!clk_intd->rate_max) {
		pr_err("Cannot allocate memory for rate_max\n");
		return -ENOMEM;
	}

	array = devm_kzalloc(&pdev->dev,
			prop_len * sizeof(u32) * num, GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	ret = of_property_read_u32_array(of, prop_name, array, prop_len * num);
	if (ret)
		return -ENOMEM;

	for (i = 0; i < prop_len; i++) {
		clk_intd->rate_max[i] = array[num * i];
		for (j = 1; j < num; j++)
			vdd->vdd_uv[(num - 1) * i + (j - 1)] =
					array[num * i + j];
	}

	devm_kfree(&pdev->dev, array);
	vdd->num_levels = prop_len;
	vdd->cur_level = prop_len;
	vdd->use_max_uV = true;
	clk_intd->num_rate_max = prop_len;

	return 0;
}

static void cpu_cc_get_speed_bin(struct platform_device *pdev,
			int *bin, int *version)
{
	struct resource *res;
	u32 pte_efuse;
	void __iomem *base;

	*bin = 0;
	*version = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (!res) {
		dev_info(&pdev->dev,
			"No speed/PVS binning available. Defaulting to 0!\n");
		return;
	}

	base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!base) {
		dev_info(&pdev->dev,
			"Unable to read efuse data. Defaulting to 0!\n");
		return;
	}

	pte_efuse = readl_relaxed(base);
	devm_iounmap(&pdev->dev, base);

	*bin = (pte_efuse >> 2) & 0x7;

	dev_info(&pdev->dev, "PVS version: %d bin: %d\n", *version, *bin);
}

static int cpu_cc_parse_devicetree(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk *clk;
	struct resource *res;
	char pll_name[] = "xxx-pll";
	char rcg_name[] = "xxx-mux";
	char spm_name[] = "spm_xxx_base";
	int mux_id;
	void __iomem *base;

	/* Sources of the PLL */
	clk = devm_clk_get(dev, "xo_a");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get xo_a clock\n");
		return PTR_ERR(clk);
	}

	clk = devm_clk_get(dev, "aux_clk_2");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get gpll4_ao clock\n");
		return PTR_ERR(clk);
	}
	cpu_data.aux2_rates[AUX_FULL] = clk_get_rate(clk);
	cpu_data.aux2_rates[AUX_DIV2] = cpu_data.aux2_rates[AUX_FULL] / 2;


	clk = devm_clk_get(dev, "aux_clk_3");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(dev, "Unable to get gpll0 clock\n");
		return PTR_ERR(clk);
	}
	cpu_data.aux3_rates[AUX_FULL] = clk_get_rate(clk);
	cpu_data.aux3_rates[AUX_DIV2] = cpu_data.aux3_rates[AUX_FULL] / 2;

	/* HF PLL Analog Supply */
	vdd_hf.regulator[0] = devm_regulator_get(dev, "vdd_hf_pll");
	if (IS_ERR(vdd_hf.regulator[0])) {
		if (!(PTR_ERR(vdd_hf.regulator[0]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_hf_pll regulator\n");
		return PTR_ERR(vdd_hf.regulator[0]);
	}

	/* HF PLL core logic */
	vdd_hf.regulator[1] = devm_regulator_get(dev, "vdd_mx_hf");
	if (IS_ERR(vdd_hf.regulator[1])) {
		if (!(PTR_ERR(vdd_hf.regulator[1]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_mx_hf regulator\n");
		return PTR_ERR(vdd_hf.regulator[1]);
	}
	vdd_hf.use_max_uV = true;

	/* SR PLLs core logic */
	vdd_mx_sr.regulator[0] = devm_regulator_get(dev, "vdd_mx_sr");
	if (IS_ERR(vdd_mx_sr.regulator[0])) {
		if (!(PTR_ERR(vdd_mx_sr.regulator[0]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_mx_sr regulator\n");
		return PTR_ERR(vdd_mx_sr.regulator[0]);
	}
	vdd_mx_sr.use_max_uV = true;

	vdd_cpu_a72.regulator[0] = devm_regulator_get(dev, "vdd_a72");
	if (IS_ERR(vdd_cpu_a72.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu_a72.regulator[0]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_a72 regulator\n");
		return PTR_ERR(vdd_cpu_a72.regulator[0]);
	}

	vdd_cpu_a53.regulator[0] = devm_regulator_get(dev, "vdd_a53");
	if (IS_ERR(vdd_cpu_a53.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu_a53.regulator[0]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_a53 regulator\n");
		return PTR_ERR(vdd_cpu_a53.regulator[0]);
	}
	vdd_cpu_a53.use_max_uV = true;

	vdd_cpu_cci.regulator[0] = devm_regulator_get(dev, "vdd_cci");
	if (IS_ERR(vdd_cpu_cci.regulator[0])) {
		if (!(PTR_ERR(vdd_cpu_cci.regulator[0]) == -EPROBE_DEFER))
			dev_err(dev, "Unable to get vdd_cci regulator\n");
		return PTR_ERR(vdd_cpu_cci.regulator[0]);
	}
	vdd_cpu_cci.use_max_uV = true;

	/* PLL */
	for (mux_id = 0; mux_id <= APCS_CCI_PLL_BASE; mux_id++) {
		snprintf(pll_name, ARRAY_SIZE(pll_name), "%s-pll",
						mux_names[mux_id]);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, pll_name);
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			dev_err(dev, "Failed to map register base\n");
			return PTR_ERR(base);
		}

		cpu_ccpll[mux_id]->clkr.regmap = devm_regmap_init_mmio(dev,
						base, &cpu_regmap_config);
		if (IS_ERR(cpu_ccpll[mux_id]->clkr.regmap)) {
			dev_err(dev, "Cannot init regmap MMIO\n");
			return PTR_ERR(cpu_ccpll[mux_id]->clkr.regmap);
		};
	}

	/* MUX */
	for (mux_id = 0; mux_id < A53SS_MUX_NUM; mux_id++) {
		snprintf(rcg_name, ARRAY_SIZE(rcg_name), "%s-mux",
						mux_names[mux_id]);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, rcg_name);
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base)) {
			dev_err(dev, "Failed to map apcs_cpu_pll register base\n");
			return PTR_ERR(base);
		}

		cpu_cc_mux[mux_id]->clkr.regmap = devm_regmap_init_mmio(dev,
						base, &cpu_regmap_config);
		if (IS_ERR(cpu_cc_mux[mux_id]->clkr.regmap)) {
			dev_err(dev, "Cannot init a53ssmux regmap MMIO\n"); //CHECKME
			return PTR_ERR(cpu_cc_mux[mux_id]->clkr.regmap);
		};
	}

	for (mux_id = 0; mux_id <= APCS_CCI_PLL_BASE; mux_id++) {
		snprintf(spm_name, ARRAY_SIZE(spm_name), "spm_%s_base",
				mux_names[mux_id]);
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, spm_name);
		if (!res) {
			dev_err(dev, "Register base not defined\n");
			return -ENOMEM;
		}

		cpu_ccpll_data[mux_id]->spm_iobase = devm_ioremap(dev,
						res->start, resource_size(res));
		if(!cpu_ccpll_data[mux_id]->spm_iobase) {
			dev_err(dev, "Failed to ioremap spm registers\n");
			return -ENOMEM;
		}
	}

	return 0;
};

/**
 * clock_panic_callback() - panic notification callback function.
 *		This function is invoked when a kernel panic occurs.
 * @nfb:	Notifier block pointer
 * @event:	Value passed unmodified to notifier function
 * @data:	Pointer passed unmodified to notifier function
 *
 * Return: NOTIFY_OK
 */
static int clock_panic_callback(struct notifier_block *nfb,
			unsigned long event, void *data)
{
	unsigned long rate;

	rate = clk_hw_is_enabled(&a53ssmux.clkr.hw) ?
				clk_hw_get_rate(&a53ssmux.clkr.hw) : 0;
	pr_err("%s: %s frequency %10lu Hz\n", __func__,
				clk_hw_get_name(&a53ssmux.clkr.hw), rate);

	rate = clk_hw_is_enabled(&a72ssmux.clkr.hw) ?
				clk_hw_get_rate(&a72ssmux.clkr.hw) : 0;
	pr_err("%s: %s frequency %10lu Hz\n", __func__,
				clk_hw_get_name(&a72ssmux.clkr.hw), rate);

	rate = clk_hw_is_enabled(&ccissmux.clkr.hw) ?
				clk_hw_get_rate(&ccissmux.clkr.hw) : 0;
	pr_err("%s: %s frequency %10lu Hz\n", __func__,
				clk_hw_get_name(&ccissmux.clkr.hw), rate);

	return NOTIFY_OK;
}

static struct notifier_block clock_panic_notifier = {
	.notifier_call = clock_panic_callback,
	.priority = 1,
};

extern int clock_rcgwr_init(struct platform_device *pdev);

static int cpu_cc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct clk_hw_onecell_data *data;
	unsigned long rate, safe_rate;
	char prop_name[] = "qcom,speedX-bin-vX-XXX";
	int cpu, i, mux_id, ret, speed_bin, version;

	/* Get device tree information */
	ret = cpu_cc_parse_devicetree(pdev);
	if (ret)
		return ret;

	/* Get speed bin information */
	cpu_cc_get_speed_bin(pdev, &speed_bin, &version);

	for (mux_id = 0; mux_id < A53SS_MUX_NUM; mux_id++) {
		snprintf(prop_name, ARRAY_SIZE(prop_name),
					"qcom,speed%d-bin-v%d-%s",
					speed_bin, version, mux_names[mux_id]);
		ret = cpu_cc_get_fmax_vdd_class(pdev, (struct clk_init_data *)
					cpu_cc_mux[mux_id]->clkr.hw.init, prop_name);
		if (ret) {
			/* Fall back to most conservative PVS table */
			dev_err(dev, "Unable to load voltage plan %s!\n",
								prop_name);
			snprintf(prop_name, ARRAY_SIZE(prop_name),
				"qcom,speed0-bin-v0-%s", mux_names[mux_id]);
			ret = cpu_cc_get_fmax_vdd_class(pdev, (struct clk_init_data *)
					cpu_cc_mux[mux_id]->clkr.hw.init, prop_name);
			if (ret) {
				dev_err(dev,
					"Unable to load safe voltage plan\n");
				return ret;
			}
			dev_info(dev, "Safe voltage plan loaded.\n");
		}
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->num = ARRAY_SIZE(cpu_clks_hws);

	/* Register clocks with clock framework */
	for (i = 0; i < ARRAY_SIZE(cpu_clks_hws); i++) {
		ret = devm_clk_hw_register(dev, cpu_clks_hws[i]);
		if (ret) {
			dev_err(dev, "Failed to register clock\n");
			return ret;
		}
		data->hws[i] = cpu_clks_hws[i];
	}

	ret = of_clk_add_hw_provider(dev->of_node, of_clk_hw_onecell_get, data);
	if (ret) {
		dev_err(dev, "CPU clock driver registeration failed\n");
		return ret;
	}

	ret = clock_rcgwr_init(pdev);
	if (ret)
		dev_err(dev, "Failed to init RCGwR\n");

	for (mux_id = 0; mux_id <= APCS_CCI_PLL_BASE; mux_id++) {
		ret = clk_notifier_register(cpu_ccpll[mux_id]->clkr.hw.clk,
						&cpu_cc_mux[mux_id]->clk_nb);
		if (ret) {
			dev_err(dev, "failed to register clock notifier: %d\n", ret);
			return ret;
		}
	}

	/*
	 * We don't want the CPU clocks to be turned off at late init
	 * if CPUFREQ or HOTPLUG configs are disabled. So, bump up the
	 * refcount of these clocks. Any cpufreq/hotplug manager can assume
	 * that the clocks have already been prepared and enabled by the time
	 * they take over.
	 */
	get_online_cpus();

	for_each_possible_cpu(cpu) {
		if (cpu <= 3)
			cpumask_set_cpu(cpu, &cpu_data.a53ssmux_desc.cpumask);
		else
			cpumask_set_cpu(cpu, &cpu_data.a72ssmux_desc.cpumask);
	}

	/* Put proxy vote for PLLs */
	WARN(clk_prepare_enable(cci_sr_pll.clkr.hw.clk),
				"Unable to Turn on CCI PLL");
	WARN(clk_prepare_enable(a53ss_sr_pll.clkr.hw.clk),
				"Unable to Turn on A53 PLL");
	WARN(clk_prepare_enable(a72ss_hf_pll.clkr.hw.clk),
				"Unable to Turn on A72 PLL");

	/* Reconfigure APSS RCG */
	for (mux_id = 0; mux_id < A53SS_MUX_NUM; mux_id++) {
		rate = clk_hw_get_rate(&cpu_cc_mux[mux_id]->clkr.hw);
		if (!rate) {
			safe_rate = cpu_cc_mux[mux_id]->safe_freq;
			if (!safe_rate)
				panic("Can't get %s safe rate! Panic.\n",
					cpu_cc_mux[mux_id]->clkr.hw.init->name);

			dev_err(dev, "Unknown %s rate. Setting safe rate %ld\n",
						cpu_cc_mux[mux_id]->clkr.hw.init->name, safe_rate);
			ret = clk_set_rate((struct clk *)&cpu_cc_mux[mux_id]->clkr.hw, safe_rate);
			if (ret)
				dev_err(dev, "Can't set safe rate\n");
		}
	}

	for_each_online_cpu(cpu) {
		WARN(clk_prepare_enable(ccissmux.clkr.hw.clk),
				"Unable to Turn on CCI clock");
		WARN(clk_prepare_enable(a53ssmux.clkr.hw.clk),
				"Unable to Turn on A53 clock");
		if (cpu >= 4)
			WARN(clk_prepare_enable(a72ssmux.clkr.hw.clk),
					"Unable to Turn on A72 clock");
	}

	put_online_cpus();

	/* Remove proxy vote for PLLs */
	for (mux_id = 0; mux_id <= APCS_CCI_PLL_BASE; mux_id++) {
		clk_disable_unprepare(cpu_ccpll[mux_id]->clkr.hw.clk);
	}

	cpu_cc_populate_opp_table(pdev);

	atomic_notifier_chain_register(&panic_notifier_list,
					&clock_panic_notifier);

	dev_info(dev, "CPU clock Driver probed successfully\n");

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,cpu-msm8976" },
	{ }
};

static struct platform_driver cpu_clk_driver = {
	.probe = cpu_cc_probe,
	.driver = {
		.name = "qcom,cpu-msm8976",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init cpu_cc_init(void)
{

	return platform_driver_register(&cpu_clk_driver);
}
subsys_initcall(cpu_cc_init);

static void __exit cpu_cc_exit(void)
{
	platform_driver_unregister(&cpu_clk_driver);
}
module_exit(cpu_cc_exit);

#define APCS_ALIAS1_CMD_RCGR		0xb011050
#define APCS_ALIAS1_CFG_OFF		0x4
#define APCS_ALIAS1_CORE_CBCR_OFF	0x8
#define SRC_SEL				0x4
#define SRC_DIV				0x1

static int __init cpu_clock_a72_init(void)
{
	void __iomem  *base;
	int regval = 0, count;
	struct device_node *ofnode = of_find_compatible_node(NULL, NULL,
							"qcom,cpu-msm8976");

	if (!ofnode)
		return 0;

	base = ioremap_nocache(APCS_ALIAS1_CMD_RCGR, SZ_8);
	regval = readl_relaxed(base);

	/* Source GPLL0 and at the rate of GPLL0 */
	regval = (SRC_SEL << 8) | SRC_DIV; /* 0x401 */
	writel_relaxed(regval, base + APCS_ALIAS1_CFG_OFF);
	/* Make sure src sel and src div is set before update bit */
	mb();

	/* update bit */
	regval = readl_relaxed(base);
	regval |= BIT(0);
	writel_relaxed(regval, base);

	/* Wait for update to take effect */
	for (count = 500; count > 0; count--) {
		if ((!(readl_relaxed(base))) & BIT(0))
			break;
		udelay(1);
	}
	if ((!(readl_relaxed(base))) & BIT(0))
		panic("A72 RCG configuration didn't update!\n");

	/* Enable the branch */
	regval =  readl_relaxed(base + APCS_ALIAS1_CORE_CBCR_OFF);
	regval |= BIT(0);
	writel_relaxed(regval, base + APCS_ALIAS1_CORE_CBCR_OFF);
	/* Branch enable should be complete */
	mb();
	iounmap(base);

	pr_info("A72 Power clocks configured\n");

	return 0;
}
early_initcall(cpu_clock_a72_init);

MODULE_ALIAS("platform:cpu");
MODULE_DESCRIPTION("MSM8976 CPU clock Driver");
MODULE_LICENSE("GPL v2");
