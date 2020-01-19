/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_IOMMU_HW_V2_H
#define __ARCH_ARM_MACH_MSM_IOMMU_HW_V2_H

#define CTX_SHIFT  12

#define CTX_REG(reg, base, ctx) \
	((base) + (reg) + ((ctx) << CTX_SHIFT))
#define GLB_REG(reg, base) \
	((base) + (reg))
#define GLB_REG_N(b, n, r) GLB_REG(b, ((r) + ((n) << 2)))
#define GLB_FIELD(b, r) ((b) + (r))
#define GLB_CTX_FIELD(b, c, r) (GLB_FIELD(b, r) + ((c) << CTX_SHIFT))
#define GLB_FIELD_N(b, n, r) (GLB_FIELD(b, r) + ((n) << 2))

#define GET_GLOBAL_REG(reg, base) (readl_relaxed(GLB_REG(reg, base)))
#define GET_CTX_REG(reg, base, ctx) (readl_relaxed(CTX_REG(reg, base, ctx)))

#define SET_GLOBAL_REG(reg, base, val) writel_relaxed((val), GLB_REG(reg, base))

#define SET_CTX_REG(reg, base, ctx, val) \
	writel_relaxed((val), (CTX_REG(reg, base, ctx)))

/* Wrappers for numbered registers */
#define GET_GLOBAL_REG_N(b, n, r) (readl_relaxed(GLB_REG_N(b, n, r)))

/* Field wrappers */
#define SET_GLOBAL_FIELD(b, r, F, v) \
	SET_FIELD(GLB_FIELD(b, r), r##_##F##_MASK, r##_##F##_SHIFT, (v))
#define SET_CONTEXT_FIELD(b, c, r, F, v) \
	SET_FIELD(GLB_CTX_FIELD(b, c, r),		\
			r##_##F##_MASK, r##_##F##_SHIFT, (v))

/* Wrappers for numbered field registers */
#define SET_GLOBAL_FIELD_N(b, n, r, F, v) \
	SET_FIELD(GLB_FIELD_N(b, n, r), r##_##F##_MASK, r##_##F##_SHIFT, v)

#define SET_FIELD(addr, mask, shift, v) \
do { \
	int t = readl_relaxed(addr); \
	writel_relaxed((t & ~((mask) << (shift))) + (((v) & \
			(mask)) << (shift)), addr); \
} while (0)

/* Global register space 0 setters / getters */
#define SET_GFSR(b, v)           SET_GLOBAL_REG(GFSR, (b), (v))

#define GET_GFAR(b)              GET_GLOBAL_REG(GFAR, (b))
#define GET_GFSR(b)              GET_GLOBAL_REG(GFSR, (b))
#define GET_GFSYNR0(b)           GET_GLOBAL_REG(GFSYNR0, (b))
#define GET_GFSYNR1(b)           GET_GLOBAL_REG(GFSYNR1, (b))
#define GET_GFSYNR2(b)           GET_GLOBAL_REG(GFSYNR2, (b))

/* Implementation defined register setters/getters */
#define SET_MICRO_MMU_CTRL_HALT_REQ(b, v) \
				SET_GLOBAL_FIELD(b, MICRO_MMU_CTRL, HALT_REQ, v)

#define MMU_CTRL_IDLE (MICRO_MMU_CTRL_IDLE_MASK << MICRO_MMU_CTRL_IDLE_SHIFT)

/* Context bank register setters/getters */
#define SET_SCTLR(b, c, v)       SET_CTX_REG(CB_SCTLR, (b), (c), (v))
#define SET_ACTLR(b, c, v)       SET_CTX_REG(CB_ACTLR, (b), (c), (v))
#define SET_TCR2(b, c, v)        SET_CTX_REG(CB_TCR2, (b), (c), (v))
#define SET_TTBCR(b, c, v)       SET_CTX_REG(CB_TTBCR, (b), (c), (v))
#define SET_PRRR(b, c, v)        SET_CTX_REG(CB_PRRR, (b), (c), (v))
#define SET_NMRR(b, c, v)        SET_CTX_REG(CB_NMRR, (b), (c), (v))
#define SET_PAR(b, c, v)         SET_CTX_REG(CB_PAR, (b), (c), (v))
#define SET_FSR(b, c, v)         SET_CTX_REG(CB_FSR, (b), (c), (v))
#define SET_FSRRESTORE(b, c, v)  SET_CTX_REG(CB_FSRRESTORE, (b), (c), (v))
#define SET_FAR(b, c, v)         SET_CTX_REG(CB_FAR, (b), (c), (v))
#define SET_TLBIVA(b, c, v)      SET_CTX_REG(CB_TLBIVA, (b), (c), (v))
#define SET_TLBIASID(b, c, v)    SET_CTX_REG(CB_TLBIASID, (b), (c), (v))
#define SET_TLBIVAL(b, c, v)     SET_CTX_REG(CB_TLBIVAL, (b), (c), (v))
#define SET_TLBSYNC(b, c, v)     SET_CTX_REG(CB_TLBSYNC, (b), (c), (v))

#define GET_FSR(b, c)            GET_CTX_REG(CB_FSR, (b), (c))
#define GET_FAR(b, c)            GET_CTX_REG(CB_FAR, (b), (c))

/* Context Bank Attribute 2 Register: CBA2R_N */
#define SET_CBA2R_VA64(b, n, v)     SET_GLOBAL_FIELD_N(b, n, CBA2R, VA64, v)

/* System Control Register: CB_SCTLR */
#define SET_CB_SCTLR_M(b, c, v)     SET_CONTEXT_FIELD(b, c, CB_SCTLR, M, v)
#define SET_CB_SCTLR_TRE(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_SCTLR, TRE, v)
#define SET_CB_SCTLR_AFE(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_SCTLR, AFE, v)
#define SET_CB_SCTLR_CFRE(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_SCTLR, CFRE, v)
#define SET_CB_SCTLR_CFIE(b, c, v)  SET_CONTEXT_FIELD(b, c, CB_SCTLR, CFIE, v)
#define SET_CB_SCTLR_CFCFG(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, CFCFG, v)
#define SET_CB_SCTLR_HUPCF(b, c, v) SET_CONTEXT_FIELD(b, c, CB_SCTLR, HUPCF, v)
#define SET_CB_SCTLR_ASIDPNE(b, c, v) \
			SET_CONTEXT_FIELD(b, c, CB_SCTLR, ASIDPNE, v)

/* Translation Table Base Control Register: CB_TTBCR */
/* These are shared between VMSA and LPAE */
#define SET_CB_TTBCR_EAE(b, c, v)    SET_CONTEXT_FIELD(b, c, CB_TTBCR, EAE, v)

/* Translation Control Register 2: CB_TCR2 */
#define SET_CB_TCR2_SEP(b, c, v)   SET_CONTEXT_FIELD(b, c, CB_TCR2, SEP, v)

#define SET_TTBR0(b, c, v)       SET_CTX_REG(CB_TTBR0, (b), (c), (v))
#define SET_TTBR1(b, c, v)       SET_CTX_REG(CB_TTBR1, (b), (c), (v))

#define SET_CB_MAIR0(b, c, v)        SET_CTX_REG(CB_MAIR0, (b), (c), (v))
#define SET_CB_MAIR1(b, c, v)        SET_CTX_REG(CB_MAIR1, (b), (c), (v))

/* Global Register Space 0 */
#define GFAR		(0x0040)
#define GFSR		(0x0048)
#define GFSYNR0		(0x0050)
#define GFSYNR1		(0x0054)
#define GFSYNR2		(0x0058)

/* SMMU_LOCAL */
#define SMMU_INTR_SEL_NS	(0x2000)

/* Global Register Space 1 */
#define CBAR		(0x1000)
#define CBFRSYNRA	(0x1400)
#define CBA2R		(0x1800)

/* Implementation defined Register Space */
#define MICRO_MMU_CTRL	(0x2000)

/* Stage 1 Context Bank Format */
#define CB_SCTLR	(0x000)
#define CB_ACTLR	(0x004)
#define CB_TCR2		(0x010)
#define CB_TTBR0	(0x020)
#define CB_TTBR1	(0x028)
#define CB_TTBCR	(0x030)
#define CB_PRRR		(0x038)
#define CB_MAIR0	(0x038)
#define CB_NMRR		(0x03C)
#define CB_MAIR1	(0x03C)
#define CB_PAR		(0x050)
#define CB_FSR		(0x058)
#define CB_FSRRESTORE	(0x05C)
#define CB_FAR		(0x060)
#define CB_FSYNR0	(0x068)
#define CB_FSYNR1	(0x06C)
#define CB_TLBIVA	(0x600)
#define CB_TLBIASID	(0x610)
#define CB_TLBIVAL	(0x620)
#define CB_TLBSYNC	(0x7F0)
#define CB_TLBSTATUS	(0x7F4)

/* TLB Status: CB_TLBSTATUS */
#define CB_TLBSTATUS_SACTIVE (CB_TLBSTATUS_SACTIVE_MASK << \
						CB_TLBSTATUS_SACTIVE_SHIFT)

/* Translation Table Base Control Register: CB_TTBCR */
#define CB_TTBCR_EAE_MASK          0x01

/* Context Bank Attribute 2 Register: CBA2R */
#define CBA2R_VA64_MASK		0x1

/* Implementation defined register space masks */
#define MICRO_MMU_CTRL_HALT_REQ_MASK          0x01
#define MICRO_MMU_CTRL_IDLE_MASK              0x01

/* System Control Register: CB_SCTLR */
#define CB_SCTLR_M_MASK            0x01
#define CB_SCTLR_TRE_MASK          0x01
#define CB_SCTLR_AFE_MASK          0x01
#define CB_SCTLR_CFRE_MASK         0x01
#define CB_SCTLR_CFIE_MASK         0x01
#define CB_SCTLR_CFCFG_MASK        0x01
#define CB_SCTLR_HUPCF_MASK        0x01
#define CB_SCTLR_ASIDPNE_MASK      0x01

/* TLB Status: CB_TLBSTATUS */
#define CB_TLBSTATUS_SACTIVE_MASK  0x01

/* Translation Control Register 2: CB_TCR2 */
#define CB_TCR2_SEP_MASK           0x07

/* Context Bank Attribute 2 Register: CBA2R */
#define CBA2R_VA64_SHIFT		0

/* Implementation defined register space shift */
#define MICRO_MMU_CTRL_RESERVED_SHIFT         0x00
#define MICRO_MMU_CTRL_HALT_REQ_SHIFT         0x02
#define MICRO_MMU_CTRL_IDLE_SHIFT             0x03

/* Transaction Resume: CB_RESUME */
#define CB_RESUME_TNR_SHIFT        0

/* System Control Register: CB_SCTLR */
#define CB_SCTLR_M_SHIFT            0
#define CB_SCTLR_TRE_SHIFT          1
#define CB_SCTLR_AFE_SHIFT          2
#define CB_SCTLR_CFRE_SHIFT         5
#define CB_SCTLR_CFIE_SHIFT         6
#define CB_SCTLR_CFCFG_SHIFT        7
#define CB_SCTLR_HUPCF_SHIFT        8
#define CB_SCTLR_ASIDPNE_SHIFT      12

/* TLB Status: CB_TLBSTATUS */
#define CB_TLBSTATUS_SACTIVE_SHIFT  0

/* Translation Control Register 2: CB_TCR2 */
#define CB_TCR2_SEP_SHIFT           15

/* Translation Table Base Control Register: CB_TTBCR */
#define CB_TTBCR_EAE_SHIFT          31

/* Translation Table Base Register 0/1: CB_TTBR */
#define CB_TTBR0_ASID_SHIFT         48
#define CB_TTBR1_ASID_SHIFT         48

#endif
