/*
 * Copyright (c) 2026 RT-Thread
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT titan_w25q64_ra_ospi_b

#include <stdint.h>

#include <errno.h>
#include <string.h>

#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/renesas_ra_cgc.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <soc.h>
#include <r_ospi_b.h>

LOG_MODULE_REGISTER(titan_w25q64_ra_ospi_b, CONFIG_FLASH_LOG_LEVEL);

#define W25Q64_JEDEC_ID_LEN              3U
#define W25Q64_CMD_WRITE_ENABLE          0x06U
#define W25Q64_CMD_VOLATILE_SR_WRITE_EN  0x50U
#define W25Q64_CMD_READ_STATUS1          0x05U
#define W25Q64_CMD_READ_STATUS2          0x35U
#define W25Q64_CMD_WRITE_STATUS2         0x31U
#define W25Q64_CMD_READ_DATA             0x03U
#define W25Q64_CMD_FAST_READ_QUAD_IO     0xEBU
#define W25Q64_CMD_PAGE_PROGRAM          0x02U
#define W25Q64_CMD_SECTOR_ERASE_4K       0x20U
#define W25Q64_CMD_READ_JEDEC_ID         0x9FU
#define W25Q64_CMD_READ_SFDP             0x5AU
#define W25Q64_CMD_RESET_ENABLE          0x66U
#define W25Q64_CMD_RESET_MEMORY          0x99U
#define W25Q64_SR1_BUSY                  BIT(0)
#define W25Q64_SR1_WEL                   BIT(1)
#define W25Q64_SR2_QE                    BIT(1)
/* W25Q64 0xeb Quad I/O read: 1S command, 4S 24-bit address, 8 mode clocks,
 * then dummy clocks before 4S data. The OSPI_B memory-map path emits the mode
 * clocks through CMCFG0.ADDRPCD and needs RDLATE=6 for this board at 33.3 MHz.
 */
#define W25Q64_QUAD_IO_MODE_BYTE         0xffU
#define W25Q64_QUAD_IO_XIP_LATENCY_CYCLES 6U
#define W25Q64_DIRECT_DATA_MAX           8U
#define W25Q64_ADDRESS_LEN               3U
#define W25Q64_COMMAND_LEN               1U
#define W25Q64_ERASE_VALUE               0xffU
#define W25Q64_RESET_DELAY_US            30U
#define W25Q64_POWER_UP_DELAY_MS         5U
#define W25Q64_WRITE_TIMEOUT_US          10000U
#define W25Q64_ERASE_TIMEOUT_US          500000U
#define W25Q64_POLL_INTERVAL_US          50U

enum titan_w25q64_bus_mode {
	TITAN_W25Q64_BUS_SPI,
	TITAN_W25Q64_BUS_QUAD_IO_READ,
};

struct titan_w25q64_config {
	uintptr_t base;
	size_t size;
	size_t page_size;
	size_t erase_block_size;
	uint32_t max_frequency;
	uint8_t jedec_id[W25Q64_JEDEC_ID_LEN];
	const struct device *clock_dev;
	struct clock_control_ra_subsys_cfg clock_subsys;
	const struct pinctrl_dev_config *pcfg;
};

struct titan_w25q64_data {
	ospi_b_instance_ctrl_t ctrl;
	spi_flash_cfg_t ospi_cfg;
	ospi_b_timing_setting_t timing;
	ospi_b_table_t xspi_command_set;
	ospi_b_extended_cfg_t ospi_extend;
	enum titan_w25q64_bus_mode bus_mode;
	struct k_sem lock;
};

static void titan_w25q64_clear_prefetch(const struct device *dev)
{
	struct titan_w25q64_data *data = dev->data;

	data->ctrl.p_reg->BMCTL1 = 0x03U << R_XSPI0_BMCTL1_PBUFCLRCH_Pos;
}

static const struct flash_parameters titan_w25q64_parameters = {
	.write_block_size = DT_INST_PROP(0, write_block_size),
	.erase_value = W25Q64_ERASE_VALUE,
};

static inline void titan_w25q64_pack_data(const uint8_t *src, size_t len, uint64_t *out)
{
	uint64_t packed = 0U;

	for (size_t i = 0; i < len; i++) {
		packed |= ((uint64_t)src[i]) << (i * 8U);
	}

	*out = packed;
}

static inline void titan_w25q64_unpack_data(uint64_t src, uint8_t *dst, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		dst[i] = (uint8_t)(src >> (i * 8U));
	}
}

static bool titan_w25q64_range_valid(const struct titan_w25q64_config *cfg, off_t offset,
					    size_t len)
{
	return (offset >= 0) && ((size_t)offset <= cfg->size) && (len <= (cfg->size - (size_t)offset));
}

static int titan_w25q64_set_bus_mode(const struct device *dev, enum titan_w25q64_bus_mode mode)
{
	struct titan_w25q64_data *data = dev->data;
	fsp_err_t err;

	if (data->bus_mode == mode) {
		return 0;
	}

	if (mode == TITAN_W25Q64_BUS_SPI) {
		data->ospi_cfg.address_bytes = SPI_FLASH_ADDRESS_BYTES_3;
		data->ospi_cfg.read_command = W25Q64_CMD_READ_DATA;
		data->ospi_extend.read_dummy_cycles = 0U;
		err = R_OSPI_B_SpiProtocolSet(&data->ctrl, SPI_FLASH_PROTOCOL_EXTENDED_SPI);
	} else {
		data->ospi_cfg.address_bytes = SPI_FLASH_ADDRESS_BYTES_3;
		data->ospi_cfg.read_command = W25Q64_CMD_FAST_READ_QUAD_IO;
		data->ospi_extend.read_dummy_cycles = W25Q64_QUAD_IO_XIP_LATENCY_CYCLES;
		err = R_OSPI_B_SpiProtocolSet(&data->ctrl, SPI_FLASH_PROTOCOL_1S_4S_4S);
	}

	if (err != FSP_SUCCESS) {
		return -EIO;
	}

	if (mode == TITAN_W25Q64_BUS_QUAD_IO_READ) {
		data->ctrl.p_reg->CMCFGCS[data->ctrl.channel].CMCFG0 |=
			W25Q64_QUAD_IO_MODE_BYTE << R_XSPI0_CMCFGCS_CMCFG0_ADDRPCD_Pos;
		titan_w25q64_clear_prefetch(dev);
	}

	data->bus_mode = mode;

	return 0;
}

static int titan_w25q64_direct(const struct device *dev, uint8_t command, uint32_t address,
				       uint8_t address_len, const uint8_t *tx, uint8_t *rx, size_t len,
				       uint8_t dummy_cycles)
{
	struct titan_w25q64_data *data = dev->data;
	int ret;
	spi_flash_direct_transfer_t transfer = {
		.command = command,
		.address = address,
		.command_length = W25Q64_COMMAND_LEN,
		.address_length = address_len,
		.data_length = len,
		.dummy_cycles = dummy_cycles,
	};
	fsp_err_t err;

	ret = titan_w25q64_set_bus_mode(dev, TITAN_W25Q64_BUS_SPI);
	if (ret != 0) {
		return ret;
	}

	if (len > W25Q64_DIRECT_DATA_MAX) {
		return -EINVAL;
	}

	if (rx != NULL) {
		err = R_OSPI_B_DirectTransfer(&data->ctrl, &transfer, SPI_FLASH_DIRECT_TRANSFER_DIR_READ);
		if (err == FSP_SUCCESS) {
			titan_w25q64_unpack_data(transfer.data_u64, rx, len);
		}
	} else {
		if (tx != NULL) {
			titan_w25q64_pack_data(tx, len, &transfer.data_u64);
		}
		err = R_OSPI_B_DirectTransfer(&data->ctrl, &transfer, SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE);
	}

	return (err == FSP_SUCCESS) ? 0 : -EIO;
}


static int titan_w25q64_read_status1(const struct device *dev, uint8_t *status)
{
	return titan_w25q64_direct(dev, W25Q64_CMD_READ_STATUS1, 0U, 0U, NULL, status, 1U, 0U);
}

static int titan_w25q64_read_status2(const struct device *dev, uint8_t *status)
{
	return titan_w25q64_direct(dev, W25Q64_CMD_READ_STATUS2, 0U, 0U, NULL, status, 1U, 0U);
}

static int titan_w25q64_wait_ready(const struct device *dev, uint32_t timeout_us)
{
	uint8_t status = W25Q64_SR1_BUSY;

	while ((status & W25Q64_SR1_BUSY) != 0U) {
		int ret = titan_w25q64_read_status1(dev, &status);

		if (ret != 0) {
			return ret;
		}

		if ((status & W25Q64_SR1_BUSY) == 0U) {
			return 0;
		}

		if (timeout_us < W25Q64_POLL_INTERVAL_US) {
			return -ETIMEDOUT;
		}

		timeout_us -= W25Q64_POLL_INTERVAL_US;
		k_sleep(K_USEC(W25Q64_POLL_INTERVAL_US));
	}

	return 0;
}

static int titan_w25q64_write_enable(const struct device *dev)
{
	uint8_t status;
	int ret;

	ret = titan_w25q64_direct(dev, W25Q64_CMD_WRITE_ENABLE, 0U, 0U, NULL, NULL, 0U, 0U);
	if (ret != 0) {
		return ret;
	}

	ret = titan_w25q64_read_status1(dev, &status);
	if (ret != 0) {
		return ret;
	}

	return ((status & W25Q64_SR1_WEL) != 0U) ? 0 : -EIO;
}

static int titan_w25q64_read_jedec_unlocked(const struct device *dev, uint8_t *id)
{
	return titan_w25q64_direct(dev, W25Q64_CMD_READ_JEDEC_ID, 0U, 0U, NULL, id,
				     W25Q64_JEDEC_ID_LEN, 0U);
}

static int titan_w25q64_reset_unlocked(const struct device *dev)
{
	int ret;

	ret = titan_w25q64_set_bus_mode(dev, TITAN_W25Q64_BUS_SPI);
	if (ret != 0) {
		return ret;
	}

	ret = titan_w25q64_direct(dev, W25Q64_CMD_RESET_ENABLE, 0U, 0U, NULL, NULL, 0U, 0U);
	if (ret != 0) {
		return ret;
	}

	ret = titan_w25q64_direct(dev, W25Q64_CMD_RESET_MEMORY, 0U, 0U, NULL, NULL, 0U, 0U);
	if (ret != 0) {
		return ret;
	}

	k_sleep(K_USEC(W25Q64_RESET_DELAY_US));

	return titan_w25q64_wait_ready(dev, W25Q64_WRITE_TIMEOUT_US);
}

static int titan_w25q64_enable_quad_unlocked(const struct device *dev)
{
	uint8_t status2;
	uint8_t value;
	int ret;

	ret = titan_w25q64_set_bus_mode(dev, TITAN_W25Q64_BUS_SPI);
	if (ret != 0) {
		return ret;
	}

	ret = titan_w25q64_read_status2(dev, &status2);
	if (ret != 0) {
		return ret;
	}

	if ((status2 & W25Q64_SR2_QE) != 0U) {
		return 0;
	}

	ret = titan_w25q64_direct(dev, W25Q64_CMD_VOLATILE_SR_WRITE_EN, 0U, 0U, NULL, NULL, 0U, 0U);
	if (ret != 0) {
		return ret;
	}

	value = status2 | W25Q64_SR2_QE;
	ret = titan_w25q64_direct(dev, W25Q64_CMD_WRITE_STATUS2, 0U, 0U, &value, NULL, 1U, 0U);
	if (ret != 0) {
		return ret;
	}

	ret = titan_w25q64_wait_ready(dev, W25Q64_WRITE_TIMEOUT_US);
	if (ret != 0) {
		return ret;
	}

	ret = titan_w25q64_read_status2(dev, &status2);
	if (ret != 0) {
		return ret;
	}

	return ((status2 & W25Q64_SR2_QE) != 0U) ? 0 : -EIO;
}

static int titan_w25q64_read(const struct device *dev, off_t offset, void *dest, size_t len)
{
	const struct titan_w25q64_config *cfg = dev->config;
	struct titan_w25q64_data *data = dev->data;
	int ret = 0;

	if (len == 0U) {
		return 0;
	}

	if (dest == NULL) {
		return -EINVAL;
	}

	if (!titan_w25q64_range_valid(cfg, offset, len)) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	ret = titan_w25q64_set_bus_mode(dev, TITAN_W25Q64_BUS_QUAD_IO_READ);
	if (ret == 0) {
		const void *src = (const void *)(cfg->base + (uintptr_t)offset);

		(void)sys_cache_data_invd_range((void *)src, len);
		titan_w25q64_clear_prefetch(dev);
		memcpy(dest, src, len);
	}

	k_sem_give(&data->lock);

	return ret;
}

static int titan_w25q64_write(const struct device *dev, off_t offset, const void *src, size_t len)
{
	const struct titan_w25q64_config *cfg = dev->config;
	struct titan_w25q64_data *data = dev->data;
	const uint8_t *buf = src;
	int ret = 0;

	if (len == 0U) {
		return 0;
	}

	if (src == NULL) {
		return -EINVAL;
	}

	if (!titan_w25q64_range_valid(cfg, offset, len)) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	while (len > 0U) {
		size_t page_remaining = cfg->page_size - ((size_t)offset % cfg->page_size);
		size_t chunk = MIN(MIN(len, W25Q64_DIRECT_DATA_MAX), page_remaining);

		ret = titan_w25q64_write_enable(dev);
		if (ret != 0) {
			break;
		}

		ret = titan_w25q64_direct(dev, W25Q64_CMD_PAGE_PROGRAM, (uint32_t)offset,
					     W25Q64_ADDRESS_LEN, buf, NULL, chunk, 0U);
		if (ret != 0) {
			break;
		}

		ret = titan_w25q64_wait_ready(dev, W25Q64_WRITE_TIMEOUT_US);
		if (ret != 0) {
			break;
		}

		(void)sys_cache_data_invd_range((void *)(cfg->base + (uintptr_t)offset), chunk);
		titan_w25q64_clear_prefetch(dev);

		buf += chunk;
		offset += chunk;
		len -= chunk;
	}

	k_sem_give(&data->lock);

	return ret;
}

static int titan_w25q64_erase(const struct device *dev, off_t offset, size_t len)
{
	const struct titan_w25q64_config *cfg = dev->config;
	struct titan_w25q64_data *data = dev->data;
	int ret = 0;

	if (len == 0U) {
		return 0;
	}

	if (!titan_w25q64_range_valid(cfg, offset, len)) {
		return -EINVAL;
	}

	if ((((size_t)offset % cfg->erase_block_size) != 0U) ||
	    ((len % cfg->erase_block_size) != 0U)) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	while (len > 0U) {
		ret = titan_w25q64_write_enable(dev);
		if (ret != 0) {
			break;
		}

		ret = titan_w25q64_direct(dev, W25Q64_CMD_SECTOR_ERASE_4K, (uint32_t)offset,
					     W25Q64_ADDRESS_LEN, NULL, NULL, 0U, 0U);
		if (ret != 0) {
			break;
		}

		ret = titan_w25q64_wait_ready(dev, W25Q64_ERASE_TIMEOUT_US);
		if (ret != 0) {
			break;
		}

		(void)sys_cache_data_invd_range((void *)(cfg->base + (uintptr_t)offset),
					       cfg->erase_block_size);
		titan_w25q64_clear_prefetch(dev);

		offset += cfg->erase_block_size;
		len -= cfg->erase_block_size;
	}

	k_sem_give(&data->lock);

	return ret;
}

static const struct flash_parameters *titan_w25q64_get_parameters(const struct device *dev)
{
	ARG_UNUSED(dev);

	return &titan_w25q64_parameters;
}

static int titan_w25q64_get_size(const struct device *dev, uint64_t *size)
{
	const struct titan_w25q64_config *cfg = dev->config;

	if (size == NULL) {
		return -EINVAL;
	}

	*size = cfg->size;

	return 0;
}

#if defined(CONFIG_FLASH_PAGE_LAYOUT)
static void titan_w25q64_page_layout(const struct device *dev,
				     const struct flash_pages_layout **layout,
				     size_t *layout_size)
{
	static const struct flash_pages_layout flash_layout = {
		.pages_count = DT_INST_REG_SIZE(0) / DT_INST_PROP(0, erase_block_size),
		.pages_size = DT_INST_PROP(0, erase_block_size),
	};

	ARG_UNUSED(dev);

	*layout = &flash_layout;
	*layout_size = 1U;
}
#endif

#if defined(CONFIG_FLASH_JESD216_API)
static int titan_w25q64_read_jedec_id(const struct device *dev, uint8_t *id)
{
	struct titan_w25q64_data *data = dev->data;
	int ret;

	if (id == NULL) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);
	ret = titan_w25q64_read_jedec_unlocked(dev, id);
	k_sem_give(&data->lock);

	return ret;
}

static int titan_w25q64_sfdp_read(const struct device *dev, off_t offset, void *dest, size_t len)
{
	struct titan_w25q64_data *data = dev->data;
	uint8_t *dst = dest;
	int ret = 0;

	if (len == 0U) {
		return 0;
	}

	if ((offset < 0) || (dest == NULL)) {
		return -EINVAL;
	}

	k_sem_take(&data->lock, K_FOREVER);

	while (len > 0U) {
		size_t chunk = MIN(len, W25Q64_DIRECT_DATA_MAX);

		ret = titan_w25q64_direct(dev, W25Q64_CMD_READ_SFDP, (uint32_t)offset,
					     W25Q64_ADDRESS_LEN, NULL, dst, chunk, 8U);
		if (ret != 0) {
			break;
		}

		dst += chunk;
		offset += chunk;
		len -= chunk;
	}

	k_sem_give(&data->lock);

	return ret;
}
#endif

#if defined(CONFIG_FLASH_EX_OP_ENABLED)
static int titan_w25q64_ex_op(const struct device *dev, uint16_t code, const uintptr_t in, void *out)
{
	struct titan_w25q64_data *data = dev->data;
	int ret;

	ARG_UNUSED(in);
	ARG_UNUSED(out);

	if (code != FLASH_EX_OP_RESET) {
		return -ENOTSUP;
	}

	k_sem_take(&data->lock, K_FOREVER);
	ret = titan_w25q64_reset_unlocked(dev);
	if (ret == 0) {
		ret = titan_w25q64_enable_quad_unlocked(dev);
	}
	if (ret == 0) {
		ret = titan_w25q64_set_bus_mode(dev, TITAN_W25Q64_BUS_QUAD_IO_READ);
	}
	k_sem_give(&data->lock);

	return ret;
}
#endif

static DEVICE_API(flash, titan_w25q64_api) = {
	.read = titan_w25q64_read,
	.write = titan_w25q64_write,
	.erase = titan_w25q64_erase,
	.get_parameters = titan_w25q64_get_parameters,
	.get_size = titan_w25q64_get_size,
#if defined(CONFIG_FLASH_PAGE_LAYOUT)
	.page_layout = titan_w25q64_page_layout,
#endif
#if defined(CONFIG_FLASH_JESD216_API)
	.sfdp_read = titan_w25q64_sfdp_read,
	.read_jedec_id = titan_w25q64_read_jedec_id,
#endif
#if defined(CONFIG_FLASH_EX_OP_ENABLED)
	.ex_op = titan_w25q64_ex_op,
#endif
};

static int titan_w25q64_init(const struct device *dev)
{
	const struct titan_w25q64_config *cfg = dev->config;
	struct titan_w25q64_data *data = dev->data;
	uint32_t clock_freq;
	uint8_t id[W25Q64_JEDEC_ID_LEN];
	int ret;
	fsp_err_t err;

	if (!device_is_ready(cfg->clock_dev)) {
		LOG_ERR("OSPI clock device not ready");
		return -ENODEV;
	}

	ret = clock_control_on(cfg->clock_dev, (clock_control_subsys_t)&cfg->clock_subsys);
	if (ret != 0) {
		LOG_ERR("OSPI clock enable failed: %d", ret);
		return ret;
	}

	ret = clock_control_get_rate(cfg->clock_dev, (clock_control_subsys_t)&cfg->clock_subsys,
				     &clock_freq);
	if (ret != 0) {
		LOG_ERR("OSPI clock rate read failed: %d", ret);
		return ret;
	}

	if (clock_freq > cfg->max_frequency) {
		LOG_ERR("OSPI clock %u exceeds flash limit %u", clock_freq, cfg->max_frequency);
		return -EINVAL;
	}

	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		LOG_ERR("OSPI pinctrl apply failed: %d", ret);
		return ret;
	}

	k_sem_init(&data->lock, 1, 1);

	k_sleep(K_MSEC(W25Q64_POWER_UP_DELAY_MS));

	err = R_OSPI_B_Open(&data->ctrl, &data->ospi_cfg);
	if (err != FSP_SUCCESS) {
		LOG_ERR("R_OSPI_B_Open failed: %d", err);
		return -EIO;
	}

	err = R_OSPI_B_SpiProtocolSet(&data->ctrl, SPI_FLASH_PROTOCOL_EXTENDED_SPI);
	if (err != FSP_SUCCESS) {
		LOG_ERR("R_OSPI_B_SpiProtocolSet failed: %d", err);
		return -EIO;
	}

	ret = titan_w25q64_reset_unlocked(dev);
	if (ret != 0) {
		LOG_ERR("W25Q64 reset failed: %d", ret);
		return ret;
	}

	ret = titan_w25q64_read_jedec_unlocked(dev, id);
	if (ret != 0) {
		LOG_ERR("W25Q64 JEDEC read failed: %d", ret);
		return ret;
	}

	if (memcmp(id, cfg->jedec_id, sizeof(id)) != 0) {
		LOG_ERR("Unexpected JEDEC ID %02x %02x %02x", id[0], id[1], id[2]);
		return -ENODEV;
	}

	ret = titan_w25q64_enable_quad_unlocked(dev);
	if (ret != 0) {
		LOG_ERR("W25Q64 Quad enable failed: %d", ret);
		return ret;
	}

	ret = titan_w25q64_set_bus_mode(dev, TITAN_W25Q64_BUS_QUAD_IO_READ);
	if (ret != 0) {
		LOG_ERR("W25Q64 Quad memory-map setup failed: %d", ret);
		return ret;
	}

	LOG_INF("W25Q64 ready: %zu bytes, OSPI clock %u Hz, Quad I/O memory-map enabled",
		cfg->size, clock_freq);

	return 0;
}

PINCTRL_DT_DEFINE(DT_INST_PARENT(0));

static const struct titan_w25q64_config titan_w25q64_config = {
	.base = DT_INST_REG_ADDR(0),
	.size = DT_INST_REG_SIZE(0),
	.page_size = DT_INST_PROP(0, page_size),
	.erase_block_size = DT_INST_PROP(0, erase_block_size),
	.max_frequency = DT_INST_PROP(0, spi_max_frequency),
	.jedec_id = DT_INST_PROP(0, jedec_id),
	.clock_dev = DEVICE_DT_GET(DT_CLOCKS_CTLR(DT_INST_PARENT(0))),
	.clock_subsys = {
		.mstp = (uint32_t)DT_CLOCKS_CELL(DT_INST_PARENT(0), mstp),
		.stop_bit = (uint32_t)DT_CLOCKS_CELL(DT_INST_PARENT(0), stop_bit),
	},
	.pcfg = PINCTRL_DT_DEV_CONFIG_GET(DT_INST_PARENT(0)),
};

static struct titan_w25q64_data titan_w25q64_data = {
	.timing = {
		.command_to_command_interval = OSPI_B_COMMAND_INTERVAL_CLOCKS_2,
		.cs_pullup_lag = OSPI_B_COMMAND_CS_PULLUP_CLOCKS_NO_EXTENSION,
		.cs_pulldown_lead = OSPI_B_COMMAND_CS_PULLDOWN_CLOCKS_NO_EXTENSION,
	},
	.xspi_command_set = {
		.p_table = NULL,
		.length = 0U,
	},
	.ospi_extend = {
		.ospi_b_unit = 0U,
		.channel = OSPI_B_DEVICE_NUMBER_1,
		.p_timing_settings = &titan_w25q64_data.timing,
		.p_xspi_command_set = &titan_w25q64_data.xspi_command_set,
		.p_autocalibration_preamble_pattern_addr = (uint8_t *)DT_INST_REG_ADDR(0),
		.data_latch_delay_clocks = 0U,
		.read_dummy_cycles = 0U,
		.program_dummy_cycles = 0U,
		.status_dummy_cycles = 0U,
	},
	.ospi_cfg = {
		.spi_protocol = SPI_FLASH_PROTOCOL_EXTENDED_SPI,
		.read_mode = SPI_FLASH_READ_MODE_STANDARD,
		.address_bytes = SPI_FLASH_ADDRESS_BYTES_3,
		.dummy_clocks = SPI_FLASH_DUMMY_CLOCKS_0,
		.page_program_address_lines = SPI_FLASH_DATA_LINES_1,
		.write_status_bit = 0U,
		.write_enable_bit = 1U,
		.page_size_bytes = DT_INST_PROP(0, page_size),
		.page_program_command = W25Q64_CMD_PAGE_PROGRAM,
		.write_enable_command = W25Q64_CMD_WRITE_ENABLE,
		.status_command = W25Q64_CMD_READ_STATUS1,
		.read_command = W25Q64_CMD_READ_DATA,
		.xip_enter_command = 0U,
		.xip_exit_command = 0U,
		.erase_command_list_length = 0U,
		.p_erase_command_list = NULL,
		.p_extend = &titan_w25q64_data.ospi_extend,
	},
};

BUILD_ASSERT(DT_INST_REG_SIZE(0) == (8U * 1024U * 1024U), "W25Q64 size must be 8 MiB");
BUILD_ASSERT(DT_INST_PROP_LEN(0, jedec_id) == W25Q64_JEDEC_ID_LEN, "W25Q64 JEDEC ID must be 3 bytes");
BUILD_ASSERT(DT_INST_PROP(0, page_size) == 256, "W25Q64 page size must be 256 bytes");
BUILD_ASSERT(DT_INST_PROP(0, erase_block_size) == 4096, "W25Q64 erase block must be 4 KiB");
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1, "Only one W25Q64 OSPI_B instance is supported");

DEVICE_DT_INST_DEFINE(0, titan_w25q64_init, NULL, &titan_w25q64_data, &titan_w25q64_config,
		      POST_KERNEL, CONFIG_FLASH_INIT_PRIORITY, &titan_w25q64_api);
