/*
 * Copyright (C) 2016 Sony Mobile Communications Inc.
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

#ifndef __INCELL_H__
#define __INCELL_H__

#include <linux/types.h>

#define INCELL_OK                ((int)0)

/* We need to discuss the bellow error name */
#define INCELL_ERROR             ((int)(-1))

#define INCELL_ALREADY_LOCKED    ((int)(-2))
#define INCELL_ALREADY_UNLOCKED  ((int)(-3))
#define INCELL_EBUSY             ((int)(-4))
#define INCELL_EALREADY          ((int)(-5))

#define INCELL_POWER_ON          ((bool)true)
#define INCELL_POWER_OFF         ((bool)false)
#define INCELL_FORCE             ((bool)true)
#define INCELL_UNFORCE           ((bool)false)

typedef struct {
	bool touch_power;
	bool display_power;
} panel_incell_pw_status;

typedef enum {
	INCELL_DISPLAY_HW_RESET,
	INCELL_DISPLAY_OFF,
	INCELL_DISPLAY_ON,
} panel_incell_intf_mode;

typedef enum {
	INCELL_DISPLAY_POWER_UNLOCK,
	INCELL_DISPLAY_POWER_LOCK,
} panel_incell_pw_lock;

typedef enum {
	INCELL_DISPLAY_EWU_DISABLE,
	INCELL_DISPLAY_EWU_ENABLE,
} panel_incell_ewu_mode;

/**
 * @brief Get incell power status.
 * @param[in] power_status : touch_power/display_power <br>
 *            INCELL_POWER_ON  : Power on state <br>
 *            INCELL_POWER_OFF : Power off state.
 * @return INCELL_OK     : Get power status successfully <br>
 *         INCELL_ERROR  : Failed to get power status.
 */
int panel_incell_get_power_status(panel_incell_pw_status *power_status);

/**
 * @brief Display control mode.
 * @param[in] mode : INCELL_DISPLAY_HW_RESET(execute on/off) <br>
 *                   INCELL_DISPLAY_OFF(touch is working) <br>
 *                   INCELL_DISPLAY_ON(only LCD on)
 * @param[in] force : INCELL_FORCE   - Forcibly execute an interface <br>
 *                    INCELL_UNFORCE - Depending power lock or not.
 * @return INCELL_OK        : success <br>
 *         INCELL_ERROR     : error detected <br>
 *         INCELL_EBUSY     : try lock failed.
 * @attention You cannot call this function from same fb_blank context.
 */
int panel_incell_control_mode(panel_incell_intf_mode mode, bool force);

/**
 * @brief LCD/Touch Power lock control.
 * @param[in] lock : INCELL_DISPLAY_POWER_UNLOCK(not keep power supply) <br>
 *                   INCELL_DISPLAY_POWER_LOCK(keep power supply) <br>
 * @param[out] power_status : return power state of incell_pw_status
 *                   by calling incell_get_power_status function.
 * @return INCELL_OK               : success <br>
 *         INCELL_ERROR            : error detected <br>
 *         INCELL_ALREADY_LOCKED   : Already power locked <br>
 *         INCELL_ALREADY_UNLOCKED : Already power unlocked.
 */
int panel_incell_power_lock_ctrl(panel_incell_pw_lock lock,
		panel_incell_pw_status *power_status);

/**
 * @brief Easy wake up mode (EWU) control.
 * @param[in] ewu : INCELL_DISPLAY_EWU_ENABLE (EWU mode enabled) <br>
 *                  INCELL_DISPLAY_EWU_DISABLE (EWU mode disabled)
 */
void panel_incell_ewu_mode_ctrl(panel_incell_ewu_mode ewu);

#endif /* __INCELL_H__ */
