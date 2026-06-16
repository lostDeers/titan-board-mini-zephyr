# Copyright (c) 2026 RT-Thread
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)
zephyr_library_sources_ifdef(CONFIG_TITAN_BOARD_MINI_OV5640_XCLK ov5640_camera_xclk.c)
