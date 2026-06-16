/*
 * Copyright (c) 2026 RT-Thread
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/init.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(titan_ov5640_xclk, CONFIG_CLOCK_CONTROL_LOG_LEVEL);

#define OV5640_XCLK_NODE DT_NODELABEL(ov5640_xclk)

BUILD_ASSERT(DT_NODE_HAS_STATUS(OV5640_XCLK_NODE, okay),
	     "CONFIG_TITAN_BOARD_MINI_OV5640_XCLK requires an okay ov5640_xclk node");
BUILD_ASSERT(CONFIG_TITAN_BOARD_MINI_OV5640_XCLK_INIT_PRIORITY >
	     CONFIG_CLOCK_CONTROL_PWM_INIT_PRIORITY,
	     "OV5640 XCLK init must run after pwm-clock init");
BUILD_ASSERT(CONFIG_TITAN_BOARD_MINI_OV5640_XCLK_INIT_PRIORITY < CONFIG_VIDEO_INIT_PRIORITY,
	     "OV5640 XCLK init must run before video sensor init");

static int titan_board_mini_ov5640_xclk_init(void)
{
	const struct device *const xclk = DEVICE_DT_GET(OV5640_XCLK_NODE);
	int ret;

	if (!device_is_ready(xclk)) {
		return -ENODEV;
	}

	ret = clock_control_on(xclk, (clock_control_subsys_t)0);
	if (ret != 0 && ret != -EALREADY) {
		return ret;
	}

	LOG_INF("OV5640 XCLK enabled from %s", xclk->name);
	return 0;
}

SYS_INIT(titan_board_mini_ov5640_xclk_init, POST_KERNEL,
	 CONFIG_TITAN_BOARD_MINI_OV5640_XCLK_INIT_PRIORITY);
