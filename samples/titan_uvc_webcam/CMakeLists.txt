# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(titan_board_mini_uvc_webcam)

include(cmake/titan_zephyr_overrides.cmake)
titan_uvc_apply_zephyr_overrides()

include(${ZEPHYR_BASE}/samples/subsys/usb/common/common.cmake)

target_sources(app PRIVATE src/main.c)
