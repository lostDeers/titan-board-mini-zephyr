/*
 * Copyright (c) 2026 RT-Thread
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ov5640_af_mipi_probe, CONFIG_LOG_DEFAULT_LEVEL);

#define OV5640_NODE DT_NODELABEL(ov5640)

#define OV5640_PROBE_MAGIC 0x4f563536U

struct ov5640_probe_result {
	uint32_t magic;
	uint32_t step;
	int32_t ret;
	uint32_t device_ready;
	uint32_t caps_count;
	uint32_t pixelformat;
	uint32_t width;
	uint32_t height;
	int32_t get_caps_ret;
	int32_t get_format_ret;
	int32_t test_pattern_ctrl_ret;
	int32_t focus_auto_ctrl_ret;
	int32_t focus_absolute_ctrl_ret;
};

volatile struct ov5640_probe_result ov5640_probe_result = {
	.magic = OV5640_PROBE_MAGIC,
};

BUILD_ASSERT(DT_NODE_HAS_STATUS(OV5640_NODE, okay), "OV5640 probe sample requires ov5640 node");

static const struct device *const ov5640 = DEVICE_DT_GET(OV5640_NODE);

static int query_control(uint32_t id, const char *name)
{
	struct video_ctrl_query query = {
		.dev = ov5640,
		.id = id,
	};
	int ret;

	ret = video_query_ctrl(&query);
	if (ret == 0) {
		LOG_INF("%s control present: type=%u flags=0x%x", name, query.type, query.flags);
	} else {
		LOG_INF("%s control unavailable: %d", name, ret);
	}

	return ret;
}

int main(void)
{
	struct video_caps caps = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
	};
	struct video_format fmt = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
	};
	const struct video_format_cap *cap;
	int ret;

	LOG_INF("Titan Board Mini OV5640 MIPI probe");
	ov5640_probe_result.step = 1;

	if (!device_is_ready(ov5640)) {
		ov5640_probe_result.ret = -ENODEV;
		LOG_ERR("%s is not ready", ov5640->name);
		return 0;
	}
	ov5640_probe_result.device_ready = 1;
	ov5640_probe_result.step = 2;

	ret = video_get_caps(ov5640, &caps);
	ov5640_probe_result.get_caps_ret = ret;
	if (ret != 0) {
		ov5640_probe_result.ret = ret;
		LOG_ERR("video_get_caps failed: %d", ret);
		return 0;
	}
	ov5640_probe_result.step = 3;

	LOG_INF("OV5640 output caps: min_vbuf_count=%u buf_align=%zu",
		caps.min_vbuf_count, caps.buf_align);

	for (cap = caps.format_caps; cap != NULL && cap->pixelformat != 0; cap++) {
		LOG_INF("format 0x%08x: %ux%u..%ux%u step %u/%u",
			cap->pixelformat, cap->width_min, cap->height_min,
			cap->width_max, cap->height_max, cap->width_step, cap->height_step);
		ov5640_probe_result.caps_count++;
	}

	ret = video_get_format(ov5640, &fmt);
	ov5640_probe_result.get_format_ret = ret;
	if (ret == 0) {
		ov5640_probe_result.pixelformat = fmt.pixelformat;
		ov5640_probe_result.width = fmt.width;
		ov5640_probe_result.height = fmt.height;
		LOG_INF("current format 0x%08x %ux%u pitch=%u size=%u",
			fmt.pixelformat, fmt.width, fmt.height, fmt.pitch, fmt.size);
	} else {
		ov5640_probe_result.ret = ret;
		LOG_ERR("video_get_format failed: %d", ret);
	}

	ov5640_probe_result.test_pattern_ctrl_ret = query_control(VIDEO_CID_TEST_PATTERN,
								    "test-pattern");
	ov5640_probe_result.focus_auto_ctrl_ret = query_control(VIDEO_CID_FOCUS_AUTO,
								  "focus-auto");
	ov5640_probe_result.focus_absolute_ctrl_ret = query_control(VIDEO_CID_FOCUS_ABSOLUTE,
								      "focus-absolute");
	ov5640_probe_result.step = 4;

	return 0;
}
