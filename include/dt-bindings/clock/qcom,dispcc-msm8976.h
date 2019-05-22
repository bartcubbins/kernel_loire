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

#ifndef _DT_BINDINGS_CLK_MSM_DISP_CC_MSM8976_H
#define _DT_BINDINGS_CLK_MSM_DISP_CC_MSM8976_H

/* RCGs */
#define DISP_CC_ESC0_CLK_SRC		0
#define DISP_CC_ESC1_CLK_SRC		1
#define DISP_CC_MDP_CLK_SRC		2
#define DISP_CC_MDSS_BYTE0_CLK_SRC	3
#define DISP_CC_MDSS_BYTE1_CLK_SRC	4
#define DISP_CC_MDSS_PCLK0_CLK_SRC	5
#define DISP_CC_MDSS_PCLK1_CLK_SRC	6
#define DISP_CC_VSYNC_CLK_SRC		7

/* Branches */
#define DISP_CC_ESC0_CLK		8
#define DISP_CC_ESC1_CLK		9
#define DISP_CC_MDP_TBU_CLK		10
#define DISP_CC_MDP_RT_TBU_CLK		11
#define DISP_CC_MDSS_AHB_CLK		12
#define DISP_CC_MDSS_AXI_CLK		13
#define DISP_CC_MDSS_BYTE0_CLK		14
#define DISP_CC_MDSS_BYTE1_CLK		15
#define DISP_CC_MDSS_MDP_CLK		16
#define DISP_CC_MDSS_PCLK0_CLK		17
#define DISP_CC_MDSS_PCLK1_CLK		18
#define DISP_CC_MDSS_VSYNC_CLK		19

/* Voters */
#define DISP_CC_MDSS_MDP_VOTE_CLK	20
#define DISP_CC_MDSS_ROTATOR_VOTE_CLK	21

#endif
