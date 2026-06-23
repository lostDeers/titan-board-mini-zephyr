/*
 * Copyright (c) 2026 RT-Thread
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT titan_ra8p1_mipi_csi_vin

#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <soc.h>

#include "fsp/r_mipi_csi.h"
#include "fsp/r_vin.h"
#include "video_common.h"

void vin_status_isr(void);
void vin_error_isr(void);
void mipi_csi_rx_isr(void);
void mipi_csi_dl_isr(void);
void mipi_csi_vc_isr(void);
void mipi_csi_pm_isr(void);
void mipi_csi_gst_isr(void);

LOG_MODULE_REGISTER(titan_ra_mipi_csi, CONFIG_VIDEO_LOG_LEVEL);

#define TITAN_RA_MIPI_WIDTH       320U
#define TITAN_RA_MIPI_HEIGHT      240U
#define TITAN_RA_MIPI_BYTES_PIXEL 2U
#define TITAN_RA_MIPI_FRAME_SIZE  (TITAN_RA_MIPI_WIDTH * TITAN_RA_MIPI_HEIGHT * \
					   TITAN_RA_MIPI_BYTES_PIXEL)
#define TITAN_RA_MIPI_FRAME_COUNT 3U

#define TITAN_RA_OV5640_REG8(addr) ((uint32_t)(addr) | VIDEO_REG_ADDR16_DATA8)
struct titan_ra_mipi_config {
	void (*irq_config_func)(void);
	const struct device *source_dev;
	struct i2c_dt_spec sensor_i2c;
};

struct titan_ra_mipi_data {
	struct video_format fmt;
	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
	struct k_work copy_work;
	struct k_msgq doneq;
	void *doneq_buf[TITAN_RA_MIPI_FRAME_COUNT];
	const struct device *dev;
	atomic_t streaming;
	atomic_t capture_active;
	atomic_t frames;
	atomic_t dropped_no_app_buf;
	atomic_t dropped_doneq_full;
	atomic_t start_calls;
	atomic_t last_step;
	atomic_t last_ret;
	atomic_t last_fsp_err;
	atomic_t last_sensor_ret;
#ifdef CONFIG_POLL
	struct k_poll_signal *signal;
#endif
	vin_instance_ctrl_t *vin_ctrl;
	capture_cfg_t const *vin_cfg;
};

static const struct video_format_cap titan_ra_mipi_fmts[] = {
	{
		.pixelformat = VIDEO_PIX_FMT_RGB565,
		.width_min = TITAN_RA_MIPI_WIDTH,
		.width_max = TITAN_RA_MIPI_WIDTH,
		.height_min = TITAN_RA_MIPI_HEIGHT,
		.height_max = TITAN_RA_MIPI_HEIGHT,
	},
	{0},
};

static void titan_ra_mipi_csi_callback(mipi_csi_callback_args_t *args)
{
	ARG_UNUSED(args);
}


static int titan_ra_mipi_ov5640_configure_for_vin(const struct device *dev)
{
	const struct titan_ra_mipi_config *config = dev->config;
	struct titan_ra_mipi_data *data = dev->data;
	int ret;

	if (!i2c_is_ready_dt(&config->sensor_i2c)) {
		atomic_set(&data->last_sensor_ret, -ENODEV);
		return -ENODEV;
	}

	/* Match the Titan Mini BSP's known-good OV5640 MIPI timing and ISP tuning for VIN. */
	static const struct video_reg16 regs[] = {
		{0x3035, 0x12}, {0x3036, 0x8c}, {0x3037, 0x13}, {0x3108, 0x01},
		{0x300e, 0x44}, {0x4300, 0x32}, {0x501f, 0x00}, {0x4800, 0x24},
		{0x3007, 0xfb}, {0x380c, 0x08}, {0x380d, 0x50}, {0x380e, 0x04},
		{0x380f, 0xb0}, {0x460b, 0x35}, {0x460c, 0x22}, {0x4837, 0x0a},
		{0x3824, 0x01},

		/* Auto exposure and manual white-balance gains from the Titan Mini reference. */
		{0x3a0f, 0x40}, {0x3a10, 0x30}, {0x3a1b, 0x40}, {0x3a1e, 0x30},
		{0x3a11, 0x71}, {0x3a1f, 0x20}, {0x3406, 0x01}, {0x3400, 0x06},
		{0x3401, 0x80}, {0x3402, 0x04}, {0x3403, 0x00}, {0x3404, 0x06},
		{0x3405, 0x00},

		/* AWB, color matrix, sharpening, gamma, UV/brightness, and ISP controls. */
		{0x5180, 0xff}, {0x5181, 0xf2}, {0x5182, 0x00}, {0x5183, 0x14},
		{0x5184, 0x25}, {0x5185, 0x24}, {0x5186, 0x16}, {0x5187, 0x16},
		{0x5188, 0x16}, {0x5189, 0x62}, {0x518a, 0x62}, {0x518b, 0xf0},
		{0x518c, 0xb2}, {0x518d, 0x50}, {0x518e, 0x30}, {0x518f, 0x30},
		{0x5190, 0x50}, {0x5191, 0xf8}, {0x5192, 0x04}, {0x5193, 0x70},
		{0x5194, 0xf0}, {0x5195, 0xf0}, {0x5196, 0x03}, {0x5197, 0x01},
		{0x5198, 0x04}, {0x5199, 0x12}, {0x519a, 0x04}, {0x519b, 0x00},
		{0x519c, 0x06}, {0x519d, 0x82}, {0x519e, 0x38}, {0x5381, 0x1e},
		{0x5382, 0x5b}, {0x5383, 0x14}, {0x5384, 0x06}, {0x5385, 0x82},
		{0x5386, 0x88}, {0x5387, 0x7c}, {0x5388, 0x60}, {0x5389, 0x1c},
		{0x538a, 0x01}, {0x538b, 0x98}, {0x5300, 0x08}, {0x5301, 0x30},
		{0x5302, 0x5f}, {0x5303, 0x10}, {0x5304, 0x08}, {0x5305, 0x30},
		{0x5306, 0x28}, {0x5307, 0x38}, {0x5309, 0x08}, {0x530a, 0x30},
		{0x530b, 0x04}, {0x530c, 0x06}, {0x5480, 0x01}, {0x5481, 0x06},
		{0x5482, 0x12}, {0x5483, 0x24}, {0x5484, 0x4a}, {0x5485, 0x58},
		{0x5486, 0x65}, {0x5487, 0x72}, {0x5488, 0x7d}, {0x5489, 0x88},
		{0x548a, 0x92}, {0x548b, 0xa3}, {0x548c, 0xb2}, {0x548d, 0xc8},
		{0x548e, 0xdd}, {0x548f, 0xf0}, {0x5490, 0x15}, {0x5580, 0x06},
		{0x5583, 0x40}, {0x5584, 0x20}, {0x5585, 0x00}, {0x5586, 0x20},
		{0x5587, 0x00}, {0x5588, 0x01}, {0x5589, 0x10}, {0x558a, 0x00},
		{0x558b, 0xf8}, {0x501d, 0x40}, {0x5000, 0xa7}, {0x5001, 0xa3},
		{0x503d, 0x00},
		{0x4202, 0x0f},
	};

	ret = video_write_cci_multiregs16(&config->sensor_i2c, regs, ARRAY_SIZE(regs));
	atomic_set(&data->last_sensor_ret, ret);

	return ret;
}

static int titan_ra_mipi_ov5640_stream_gate(const struct device *dev, bool enable)
{
	const struct titan_ra_mipi_config *config = dev->config;
	struct titan_ra_mipi_data *data = dev->data;
	int ret;

	if (enable) {
		ret = video_write_cci_reg(&config->sensor_i2c, TITAN_RA_OV5640_REG8(0x300e), 0x44);
		if (ret < 0) {
			atomic_set(&data->last_sensor_ret, ret);
			return ret;
		}
	}

	ret = video_write_cci_reg(&config->sensor_i2c, TITAN_RA_OV5640_REG8(0x4202),
				  enable ? 0x00 : 0x0f);
	atomic_set(&data->last_sensor_ret, ret);

	return ret;
}

static void titan_ra_mipi_cache_invalidate(const void *addr, size_t size)
{
#ifdef CONFIG_CACHE_MANAGEMENT
	(void)sys_cache_data_invd_range((void *)addr, size);
#else
	ARG_UNUSED(addr);
	ARG_UNUSED(size);
#endif
}

static void titan_ra_mipi_cache_flush(const void *addr, size_t size)
{
#ifdef CONFIG_CACHE_MANAGEMENT
	(void)sys_cache_data_flush_range((void *)addr, size);
#else
	ARG_UNUSED(addr);
	ARG_UNUSED(size);
#endif
}

static int titan_ra_mipi_start_capture(const struct device *dev);


static void titan_ra_mipi_copy_work(struct k_work *work)
{
	struct titan_ra_mipi_data *data = CONTAINER_OF(work, struct titan_ra_mipi_data, copy_work);
	uint8_t *src;
	struct video_buffer *vbuf;

	while (k_msgq_get(&data->doneq, &src, K_NO_WAIT) == 0) {

		vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT);
		if (vbuf == NULL) {
			atomic_inc(&data->dropped_no_app_buf);
			continue;
		}

		if (vbuf->size < TITAN_RA_MIPI_FRAME_SIZE) {
			k_fifo_put(&data->fifo_out, vbuf);
			continue;
		}

		titan_ra_mipi_cache_invalidate(src, TITAN_RA_MIPI_FRAME_SIZE);
		if (vbuf->buffer != src) {
			memcpy(vbuf->buffer, src, TITAN_RA_MIPI_FRAME_SIZE);
			titan_ra_mipi_cache_flush(vbuf->buffer, TITAN_RA_MIPI_FRAME_SIZE);
		}

		vbuf->bytesused = TITAN_RA_MIPI_FRAME_SIZE;
		vbuf->line_offset = 0;
		vbuf->timestamp = k_uptime_get_32();
		k_fifo_put(&data->fifo_out, vbuf);
		atomic_inc(&data->frames);

#ifdef CONFIG_POLL
		if (data->signal != NULL) {
			k_poll_signal_raise(data->signal, VIDEO_BUF_DONE);
		}
#endif
	}

	if (atomic_get(&data->streaming) && !atomic_get(&data->capture_active) &&
	    k_fifo_peek_head(&data->fifo_in) != NULL) {
		(void)titan_ra_mipi_start_capture(data->dev);
	}
}

static void titan_ra_mipi_vin_callback(capture_callback_args_t *args)
{
	const struct device *dev = args->p_context;
	struct titan_ra_mipi_data *data = dev->data;
	vin_interrupt_status_t status = {.mask = args->interrupt_status};
	uint8_t *src = args->p_buffer;
	fsp_err_t err;

	if (!atomic_get(&data->streaming) || !status.bits.frame_complete || src == NULL) {
		return;
	}

	err = R_VIN_CaptureStop(data->vin_ctrl);
	atomic_set(&data->last_fsp_err, err);
	if (err != FSP_SUCCESS) {
		atomic_set(&data->last_ret, -EIO);
		return;
	}
	atomic_clear(&data->capture_active);

	if (k_msgq_put(&data->doneq, &src, K_NO_WAIT) != 0) {
		atomic_inc(&data->dropped_doneq_full);
		return;
	}

	k_work_submit(&data->copy_work);
}

static int titan_ra_mipi_get_format(const struct device *dev, struct video_format *fmt)
{
	struct titan_ra_mipi_data *data = dev->data;

	*fmt = data->fmt;
	return 0;
}

static int titan_ra_mipi_set_format(const struct device *dev, struct video_format *fmt)
{
	const struct titan_ra_mipi_config *config = dev->config;
	struct titan_ra_mipi_data *data = dev->data;
	int ret;

	if (fmt->type != VIDEO_BUF_TYPE_OUTPUT || fmt->pixelformat != VIDEO_PIX_FMT_RGB565 ||
	    fmt->width != TITAN_RA_MIPI_WIDTH || fmt->height != TITAN_RA_MIPI_HEIGHT) {
		return -ENOTSUP;
	}

	/*
	 * Program OV5640's upstream 320x240 CSI-2 scaler path, then apply the
	 * Titan Mini VIN-specific YUV422/ISP register patch below. VIN captures
	 * into the RGB565 buffers advertised to UVC.
	 */
	ret = video_set_format(config->source_dev, fmt);
	if (ret < 0) {
		return ret;
	}

	ret = titan_ra_mipi_ov5640_configure_for_vin(dev);
	if (ret < 0) {
		return ret;
	}

	fmt->pitch = TITAN_RA_MIPI_WIDTH * TITAN_RA_MIPI_BYTES_PIXEL;
	fmt->size = TITAN_RA_MIPI_FRAME_SIZE;
	data->fmt = *fmt;

	return 0;
}

static int titan_ra_mipi_get_caps(const struct device *dev, struct video_caps *caps)
{
	ARG_UNUSED(dev);
	caps->format_caps = titan_ra_mipi_fmts;

	caps->min_vbuf_count = 1;
	caps->buf_align = 128;
	return 0;
}

static int titan_ra_mipi_start_capture(const struct device *dev)
{
	const struct titan_ra_mipi_config *config = dev->config;
	struct titan_ra_mipi_data *data = dev->data;
	struct video_buffer *next_vbuf;
	fsp_err_t err;
	int ret;

	if (atomic_get(&data->capture_active)) {
		return 0;
	}

	next_vbuf = k_fifo_peek_head(&data->fifo_in);
	if (next_vbuf == NULL || next_vbuf->buffer == NULL) {
		atomic_set(&data->last_ret, -EAGAIN);
		return -EAGAIN;
	}

	if (!device_is_ready(config->source_dev)) {
		atomic_set(&data->last_ret, -ENODEV);
		return -ENODEV;
	}


	if (!data->vin_ctrl->open) {
		err = R_VIN_Open(data->vin_ctrl, data->vin_cfg);
		atomic_set(&data->last_step, 3);
		atomic_set(&data->last_fsp_err, err);
		if (err != FSP_SUCCESS) {
			atomic_set(&data->last_ret, -EIO);
			LOG_ERR("R_VIN_Open failed: %d", err);
			return -EIO;
		}
	}

	err = R_VIN_CaptureStart(data->vin_ctrl, next_vbuf->buffer);
	atomic_set(&data->last_step, 4);
	if (err != FSP_SUCCESS) {
		atomic_set(&data->last_fsp_err, err);
		atomic_set(&data->last_ret, -EIO);
		LOG_ERR("R_VIN_CaptureStart failed: %d", err);
		return -EIO;
	}
	atomic_set(&data->capture_active, 1);

	ret = video_stream_start(config->source_dev, VIDEO_BUF_TYPE_OUTPUT);
	atomic_set(&data->last_step, 5);
	if (ret < 0) {
		atomic_set(&data->last_ret, ret);
		atomic_clear(&data->capture_active);
		(void)R_VIN_Close(data->vin_ctrl);
		return ret;
	}

	ret = titan_ra_mipi_ov5640_stream_gate(dev, true);
	atomic_set(&data->last_step, 6);
	if (ret < 0) {
		atomic_set(&data->last_ret, ret);
		atomic_clear(&data->capture_active);
		(void)R_VIN_Close(data->vin_ctrl);
		return ret;
	}

	atomic_set(&data->last_ret, 0);
	return 0;
}

static int titan_ra_mipi_set_stream(const struct device *dev, bool enable, enum video_buf_type type)
{
	const struct titan_ra_mipi_config *config = dev->config;
	struct titan_ra_mipi_data *data = dev->data;
	int ret;
	atomic_inc(&data->start_calls);
	atomic_set(&data->last_step, 1);
	atomic_set(&data->last_ret, 0);
	atomic_set(&data->last_fsp_err, 0);

	if (type != VIDEO_BUF_TYPE_OUTPUT) {
		atomic_set(&data->last_ret, -EINVAL);
		return -EINVAL;
	}

	if (!enable) {
		atomic_clear(&data->streaming);
		atomic_clear(&data->capture_active);
		(void)titan_ra_mipi_ov5640_stream_gate(dev, false);
		(void)video_stream_stop(config->source_dev, type);
		if (data->vin_ctrl->open) {
			(void)R_VIN_Close(data->vin_ctrl);
		}
		return 0;
	}

	if (atomic_get(&data->streaming)) {
		return -EBUSY;
	}

	atomic_set(&data->streaming, 1);
	ret = titan_ra_mipi_start_capture(dev);
	if (ret < 0) {
		atomic_clear(&data->streaming);
		return ret;
	}
	atomic_set(&data->last_ret, 0);
	atomic_set(&data->last_step, 7);
	return 0;
}

static int titan_ra_mipi_enqueue(const struct device *dev, struct video_buffer *vbuf)
{
	struct titan_ra_mipi_data *data = dev->data;

	if (vbuf->size < TITAN_RA_MIPI_FRAME_SIZE) {
		return -EINVAL;
	}

	vbuf->bytesused = TITAN_RA_MIPI_FRAME_SIZE;
	vbuf->line_offset = 0;
	k_fifo_put(&data->fifo_in, vbuf);
	if (atomic_get(&data->streaming) && !atomic_get(&data->capture_active)) {
		return titan_ra_mipi_start_capture(dev);
	}

	return 0;
}

static int titan_ra_mipi_dequeue(const struct device *dev, struct video_buffer **buf,
					 k_timeout_t timeout)
{
	struct titan_ra_mipi_data *data = dev->data;

	*buf = k_fifo_get(&data->fifo_out, timeout);
	if (*buf == NULL) {
		return -EAGAIN;
	}

	return 0;
}

static int titan_ra_mipi_flush(const struct device *dev, bool cancel)
{
	struct titan_ra_mipi_data *data = dev->data;
	struct video_buffer *vbuf;

	if (cancel) {
		while ((vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT)) != NULL) {
			k_fifo_put(&data->fifo_out, vbuf);
		}
#ifdef CONFIG_POLL
		if (data->signal != NULL) {
			k_poll_signal_raise(data->signal, VIDEO_BUF_ABORTED);
		}
#endif
	}

	return 0;
}

#ifdef CONFIG_POLL
static int titan_ra_mipi_set_signal(const struct device *dev, struct k_poll_signal *sig)
{
	struct titan_ra_mipi_data *data = dev->data;

	data->signal = sig;
	return 0;
}
#endif

static int titan_ra_mipi_get_frmival(const struct device *dev, struct video_frmival *frmival)
{
	const struct titan_ra_mipi_config *config = dev->config;

	return video_get_frmival(config->source_dev, frmival);
}

static int titan_ra_mipi_set_frmival(const struct device *dev, struct video_frmival *frmival)
{
	const struct titan_ra_mipi_config *config = dev->config;
	int ret;

	ret = video_set_frmival(config->source_dev, frmival);
	if (ret < 0) {
		return ret;
	}

	return titan_ra_mipi_ov5640_configure_for_vin(dev);
}

static int titan_ra_mipi_enum_frmival(const struct device *dev, struct video_frmival_enum *fie)
{
	const struct titan_ra_mipi_config *config = dev->config;

	return video_enum_frmival(config->source_dev, fie);
}

static int titan_ra_mipi_init(const struct device *dev)
{
	const struct titan_ra_mipi_config *config = dev->config;
	struct titan_ra_mipi_data *data = dev->data;
	struct video_format fmt = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
		.pixelformat = VIDEO_PIX_FMT_RGB565,
		.width = TITAN_RA_MIPI_WIDTH,
		.height = TITAN_RA_MIPI_HEIGHT,
		.pitch = TITAN_RA_MIPI_WIDTH * TITAN_RA_MIPI_BYTES_PIXEL,
		.size = TITAN_RA_MIPI_FRAME_SIZE,
	};

	config->irq_config_func();
	k_fifo_init(&data->fifo_in);
	k_fifo_init(&data->fifo_out);
	k_work_init(&data->copy_work, titan_ra_mipi_copy_work);
	k_msgq_init(&data->doneq, (char *)data->doneq_buf, sizeof(void *),
		    ARRAY_SIZE(data->doneq_buf));
	data->dev = dev;
	atomic_clear(&data->streaming);
	atomic_clear(&data->capture_active);
	atomic_clear(&data->frames);
	atomic_clear(&data->dropped_no_app_buf);
	atomic_clear(&data->dropped_doneq_full);
	atomic_clear(&data->start_calls);
	atomic_clear(&data->last_step);
	atomic_clear(&data->last_ret);
	atomic_clear(&data->last_fsp_err);
	atomic_clear(&data->last_sensor_ret);
	data->fmt = fmt;

	return 0;
}

static DEVICE_API(video, titan_ra_mipi_driver_api) = {
	.get_format = titan_ra_mipi_get_format,
	.set_format = titan_ra_mipi_set_format,
	.get_caps = titan_ra_mipi_get_caps,
	.set_stream = titan_ra_mipi_set_stream,
	.enqueue = titan_ra_mipi_enqueue,
	.dequeue = titan_ra_mipi_dequeue,
	.flush = titan_ra_mipi_flush,
	.enum_frmival = titan_ra_mipi_enum_frmival,
	.set_frmival = titan_ra_mipi_set_frmival,
	.get_frmival = titan_ra_mipi_get_frmival,
#ifdef CONFIG_POLL
	.set_signal = titan_ra_mipi_set_signal,
#endif
};

#define EP_INST_NODE(inst) DT_INST_ENDPOINT_BY_ID(inst, 0, 0)
#define SOURCE_DEV(inst)  DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(EP_INST_NODE(inst)))
#define SOURCE_I2C(inst)  I2C_DT_SPEC_GET(DT_NODE_REMOTE_DEVICE(EP_INST_NODE(inst)))

#define TITAN_IRQ_ASSIGN(inst, name, event, isr)                                                     \
	R_ICU->IELSR[DT_INST_IRQ_BY_NAME(inst, name, irq)] = BSP_PRV_IELS_ENUM(event);                \
	BSP_ASSIGN_EVENT_TO_CURRENT_CORE(BSP_PRV_IELS_ENUM(event));                                   \
	IRQ_CONNECT(DT_INST_IRQ_BY_NAME(inst, name, irq),                                              \
		    DT_INST_IRQ_BY_NAME(inst, name, priority), isr, NULL, 0);                           \
	irq_enable(DT_INST_IRQ_BY_NAME(inst, name, irq))

#define TITAN_RA_MIPI_INIT(inst)                                                                    \
	static uint8_t titan_ra_mipi_frame0_##inst[TITAN_RA_MIPI_FRAME_SIZE]                         \
		__aligned(128) __attribute__((section("SDRAM.titan_ra_mipi")));                      \
	static uint8_t titan_ra_mipi_frame1_##inst[TITAN_RA_MIPI_FRAME_SIZE]                         \
		__aligned(128) __attribute__((section("SDRAM.titan_ra_mipi")));                      \
	static uint8_t titan_ra_mipi_frame2_##inst[TITAN_RA_MIPI_FRAME_SIZE]                         \
		__aligned(128) __attribute__((section("SDRAM.titan_ra_mipi")));                      \
	static void titan_ra_mipi_irq_config_##inst(void)                                             \
	{                                                                                             \
		TITAN_IRQ_ASSIGN(inst, vin_status, EVENT_VIN_IRQ, vin_status_isr);                     \
		TITAN_IRQ_ASSIGN(inst, vin_error, EVENT_VIN_ERR, vin_error_isr);                       \
		TITAN_IRQ_ASSIGN(inst, csi_rx, EVENT_MIPICSI_RX, mipi_csi_rx_isr);                     \
		TITAN_IRQ_ASSIGN(inst, csi_dl, EVENT_MIPICSI_DL, mipi_csi_dl_isr);                     \
		TITAN_IRQ_ASSIGN(inst, csi_vc, EVENT_MIPICSI_VC, mipi_csi_vc_isr);                     \
		TITAN_IRQ_ASSIGN(inst, csi_pm, EVENT_MIPICSI_PM, mipi_csi_pm_isr);                     \
		TITAN_IRQ_ASSIGN(inst, csi_gst, EVENT_MIPICSI_GST, mipi_csi_gst_isr);                  \
	}                                                                                             \
	static const mipi_phy_timing_t titan_ra_mipi_phy_timing_##inst = {                            \
		.t_init = 74999,                                                                       \
		.dphytim2_b.t_clk_prep = 9,                                                           \
		.dphytim2_b.t_clk_settle = 62,                                                        \
		.dphytim2_b.t_clk_miss = 37,                                                          \
		.dphytim3_b.t_hs_prep = 6,                                                            \
		.dphytim3_b.t_hs_sett = 24,                                                           \
		.dphytim4_b.t_clk_trail = 7,                                                          \
		.dphytim4_b.t_clk_post = 20,                                                          \
		.dphytim4_b.t_clk_pre = 1,                                                            \
		.dphytim4_b.t_clk_zero = 28,                                                          \
		.dphytim5_b.t_hs_exit = 12,                                                           \
		.dphytim5_b.t_hs_trail = 8,                                                           \
		.dphytim5_b.t_hs_zero = 19,                                                           \
		.t_lp_exit = 7,                                                                        \
	};                                                                                            \
	static mipi_phy_ctrl_t titan_ra_mipi_phy_ctrl_##inst;                                        \
	static const mipi_phy_cfg_t titan_ra_mipi_phy_cfg_##inst = {                                  \
		.pll_settings = {.div = 2, .pll_div = 0, .mul_int = 124, .mul_frac = 0},               \
		.lp_divisor = 4,                                                                       \
		.p_timing = &titan_ra_mipi_phy_timing_##inst,                                         \
		.dsi_mode = false,                                                                     \
	};                                                                                            \
	static const mipi_phy_instance_t titan_ra_mipi_phy_##inst = {                                \
		.p_ctrl = &titan_ra_mipi_phy_ctrl_##inst,                                               \
		.p_cfg = &titan_ra_mipi_phy_cfg_##inst,                                                 \
		.p_api = &g_mipi_phy,                                                                   \
	};                                                                                            \
	static mipi_csi_instance_ctrl_t titan_ra_mipi_csi_ctrl_##inst;                               \
	static const mipi_csi_cfg_t titan_ra_mipi_csi_cfg_##inst = {                                 \
		.p_mipi_phy_instance = &titan_ra_mipi_phy_##inst,                                       \
		.ctrl_data.control_0_bits.lane_count = 2,                                               \
		.ctrl_data.control_0_bits.err_frame_notify = 1,                                         \
		.ctrl_data.control_0_bits.reserved_packet_reception = 1,                                \
		.ctrl_data.control_0_bits.generic_rule_mode = 1,                                        \
		.ctrl_data.control_2_bits.frrclk = 10,                                                  \
		.ctrl_data.control_2_bits.frrskw = 10,                                                  \
		.option_data.data_type_enable = MIPI_CSI_RX_DATA_ENABLE_YUV422_8_BIT,                  \
		.interrupt_cfg.receive_cfg.ipl = DT_INST_IRQ_BY_NAME(inst, csi_rx, priority),           \
		.interrupt_cfg.receive_cfg.irq = DT_INST_IRQ_BY_NAME(inst, csi_rx, irq),                \
		.interrupt_cfg.data_lane_cfg.ipl = DT_INST_IRQ_BY_NAME(inst, csi_dl, priority),         \
		.interrupt_cfg.data_lane_cfg.irq = DT_INST_IRQ_BY_NAME(inst, csi_dl, irq),              \
		.interrupt_cfg.virtual_channel_cfg.ipl = DT_INST_IRQ_BY_NAME(inst, csi_vc, priority),   \
		.interrupt_cfg.virtual_channel_cfg.irq = DT_INST_IRQ_BY_NAME(inst, csi_vc, irq),        \
		.interrupt_cfg.power_management_cfg.ipl = DT_INST_IRQ_BY_NAME(inst, csi_pm, priority),  \
		.interrupt_cfg.power_management_cfg.irq = DT_INST_IRQ_BY_NAME(inst, csi_pm, irq),       \
		.interrupt_cfg.short_packet_cfg.ipl = DT_INST_IRQ_BY_NAME(inst, csi_gst, priority),     \
		.interrupt_cfg.short_packet_cfg.irq = DT_INST_IRQ_BY_NAME(inst, csi_gst, irq),          \
		.interrupt_cfg.data_lane_enable_mask[0] = R_MIPI_CSI_DLIE0_RULE_Msk |                  \
			R_MIPI_CSI_DLIE0_EULE_Msk | R_MIPI_CSI_DLIE0_EESE_Msk |                         \
			R_MIPI_CSI_DLIE0_ECTE_Msk,                                                      \
		.interrupt_cfg.data_lane_enable_mask[1] = R_MIPI_CSI_DLIE0_RULE_Msk |                  \
			R_MIPI_CSI_DLIE0_EULE_Msk | R_MIPI_CSI_DLIE0_EESE_Msk |                         \
			R_MIPI_CSI_DLIE0_ECTE_Msk,                                                      \
		.interrupt_cfg.virtual_channel_enable_mask[0] = R_MIPI_CSI_VCST0_MLF_Msk |             \
			R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk |                          \
			R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk |                          \
			R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk |                          \
			R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk,                          \
		.p_callback = titan_ra_mipi_csi_callback,                                             \
	};                                                                                            \
	static const mipi_csi_instance_t titan_ra_mipi_csi_##inst = {                                \
		.p_ctrl = &titan_ra_mipi_csi_ctrl_##inst,                                               \
		.p_cfg = &titan_ra_mipi_csi_cfg_##inst,                                                 \
		.p_api = &g_mipi_csi,                                                                   \
	};                                                                                            \
	static vin_instance_ctrl_t titan_ra_mipi_vin_ctrl_##inst;                                    \
	static const vin_extended_cfg_t titan_ra_mipi_vin_extend_##inst = {                           \
		.p_mipi_csi_instance = &titan_ra_mipi_csi_##inst,                                      \
		.input_ctrl.cfg_bits.color_space_convert_bypass = 0,                                  \
		.input_ctrl.cfg_bits.interlace_mode = VIN_INTERLACE_MODE_ODD_EVEN_FIELD_CAPTURE,      \
		.input_ctrl.cfg_bits.input_mode = VIN_INPUT_FORMAT_YCBCR422_8_BIT,                    \
		.input_ctrl.cfg_bits.yuv444_conversion = VIN_YUV444_CONVERSION_MODE_INTERPOLATE,      \
		.input_ctrl.cfg_bits.pixel_data_clipping = VIN_PIXEL_DATA_CLIPPING_DEFAULT,            \
		.input_ctrl.preclip.line_start = 0,                                                    \
		.input_ctrl.preclip.line_end = TITAN_RA_MIPI_HEIGHT - 1,                              \
		.input_ctrl.preclip.pixel_start = 0,                                                   \
		.input_ctrl.preclip.pixel_end = TITAN_RA_MIPI_WIDTH - 1,                              \
		.input_ctrl.csi_mode_bits.virtual_channel = 0,                                        \
		.input_ctrl.csi_mode_bits.data_type = VIN_DATA_TYPE_YUV422_8_BIT,                    \
		.input_ctrl.csi_mode_bits.sign_extend_disable = 1,                                    \
		.input_ctrl.csi_detect_bits.field_detect_enable = 1,                                  \
		.input_ctrl.csi_detect_bits.even_field_detect_enable = 1,                             \
		.input_ctrl.image_stride = TITAN_RA_MIPI_WIDTH,                                      \
		.output_ctrl.image_buffer = {titan_ra_mipi_frame0_##inst, titan_ra_mipi_frame1_##inst,\
						 titan_ra_mipi_frame2_##inst},                                 \
		.conversion_ctrl.data_mode_bits.output_data_byte_swap = 0,                            \
		.conversion_ctrl.data_mode_bits.rgb8888_alpha_value = 0xaa,                           \
		.conversion_data.yc_rgb_conversion_setting_1_bits.y_mul = 4767,                       \
		.conversion_data.yc_rgb_conversion_setting_1_bits.round_down_disable = 1,             \
		.conversion_data.yc_rgb_conversion_setting_2_bits.csub2 = 2048,                       \
		.conversion_data.yc_rgb_conversion_setting_2_bits.ysub2 = 256,                        \
		.conversion_data.yc_rgb_conversion_setting_3_bits.cgrmul2 = 3330,                     \
		.conversion_data.yc_rgb_conversion_setting_3_bits.rcrmul2 = 6537,                     \
		.conversion_data.yc_rgb_conversion_setting_4_bits.gcbmul2 = 1605,                     \
		.conversion_data.yc_rgb_conversion_setting_4_bits.bcbmul2 = 8261,                     \
		.conversion_data.uds_ctrl_bits.bilinear_advanced = 1,                                 \
		.conversion_data.uds_scale_bits.vertical_mask = 3072,                                 \
		.conversion_data.uds_scale_bits.horizontal_mask = 5461,                               \
		.conversion_data.uds_bwidth_bits.bwidth_v = 32,                                      \
		.conversion_data.uds_bwidth_bits.bwidth_h = 32,                                      \
		.conversion_data.uds_clipping_bits.cl_vsize = TITAN_RA_MIPI_WIDTH,                   \
		.conversion_data.uds_clipping_bits.cl_hsize = TITAN_RA_MIPI_HEIGHT,                  \
		.interrupt_cfg.status_enable_mask = R_VIN_IE_FME_Msk,                                \
		.interrupt_cfg.status.ipl = DT_INST_IRQ_BY_NAME(inst, vin_status, priority),          \
		.interrupt_cfg.status.irq = DT_INST_IRQ_BY_NAME(inst, vin_status, irq),               \
		.interrupt_cfg.error.ipl = DT_INST_IRQ_BY_NAME(inst, vin_error, priority),            \
		.interrupt_cfg.error.irq = DT_INST_IRQ_BY_NAME(inst, vin_error, irq),                 \
	};                                                                                            \
	static const capture_cfg_t titan_ra_mipi_vin_cfg_##inst = {                                    \
		.x_capture_start_pixel = 0xffff,                                                        \
		.x_capture_pixels = 0xffff,                                                            \
		.y_capture_start_pixel = 0xffff,                                                        \
		.y_capture_pixels = 0xffff,                                                            \
		.bytes_per_pixel = 0xff,                                                               \
		.p_callback = titan_ra_mipi_vin_callback,                                              \
		.p_context = (void *)DEVICE_DT_INST_GET(inst),                                         \
		.p_extend = &titan_ra_mipi_vin_extend_##inst,                                          \
	};                                                                                            \
	static struct titan_ra_mipi_data titan_ra_mipi_data_##inst = {                                \
		.vin_ctrl = &titan_ra_mipi_vin_ctrl_##inst,                                            \
		.vin_cfg = &titan_ra_mipi_vin_cfg_##inst,                                              \
	};                                                                                            \
	static const struct titan_ra_mipi_config titan_ra_mipi_config_##inst = {                      \
		.irq_config_func = titan_ra_mipi_irq_config_##inst,                                     \
		.source_dev = SOURCE_DEV(inst),                                                        \
		.sensor_i2c = SOURCE_I2C(inst),                                                        \
	};                                                                                            \
	DEVICE_DT_INST_DEFINE(inst, titan_ra_mipi_init, NULL, &titan_ra_mipi_data_##inst,             \
			      &titan_ra_mipi_config_##inst, POST_KERNEL,                                \
			      CONFIG_VIDEO_TITAN_RA_MIPI_CSI_INIT_PRIORITY, &titan_ra_mipi_driver_api)

DT_INST_FOREACH_STATUS_OKAY(TITAN_RA_MIPI_INIT)
