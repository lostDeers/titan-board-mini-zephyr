/*
 * Copyright (c) 2026 RT-Thread
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#define W25Q64_NODE DT_NODELABEL(w25q64)

#define W25Q64_SIZE              (8U * 1024U * 1024U)
#define W25Q64_SECTOR_SIZE       4096U
#define W25Q64_ERASE_VALUE       0xffU
#define W25Q64_TEST_SIZE         (16U * 1024U)
#define W25Q64_TEST_OFFSET       (W25Q64_SIZE - W25Q64_TEST_SIZE)
#define W25Q64_RANDOM_CHUNK      32U
#define W25Q64_RANDOM_BLOCKS     (W25Q64_TEST_SIZE / W25Q64_RANDOM_CHUNK)
#define W25Q64_RANDOM_STEP       73U
#define W25Q64_PASS_MAGIC        0x57324250U
#define W25Q64_FAIL_MAGIC        0x57324246U

BUILD_ASSERT((W25Q64_TEST_SIZE % W25Q64_SECTOR_SIZE) == 0, "test area must be erase aligned");
BUILD_ASSERT((W25Q64_TEST_SIZE % W25Q64_RANDOM_CHUNK) == 0, "test area must split into chunks");
BUILD_ASSERT((W25Q64_RANDOM_BLOCKS & (W25Q64_RANDOM_BLOCKS - 1U)) == 0, "block count must be power of two");
BUILD_ASSERT((W25Q64_RANDOM_STEP & 1U) == 1U, "random step must be odd");

struct w25q64_benchmark_result {
	uint32_t magic;
	int32_t step;
	int32_t ret;
	uint8_t jedec_id[3];
	uint8_t sfdp_header[8];
	uint64_t flash_size;
	uint32_t page_count;
	uint32_t test_offset;
	uint32_t test_size;
	uint32_t chunk_size;
	uint32_t erase_us;
	uint32_t sequential_write_us;
	uint32_t sequential_read_us;
	uint32_t random_write_us;
	uint32_t random_read_us;
	uint32_t cleanup_erase_us;
	uint32_t sequential_write_Bps;
	uint32_t sequential_read_Bps;
	uint32_t random_write_Bps;
	uint32_t random_read_Bps;
	uint32_t mismatch_index;
	uint8_t expected;
	uint8_t actual;
};

volatile struct w25q64_benchmark_result w25q64_benchmark_result __attribute__((used));

static uint8_t write_buf[W25Q64_TEST_SIZE];
static uint8_t read_buf[W25Q64_TEST_SIZE];
static uint8_t random_chunk[W25Q64_RANDOM_CHUNK];

static uint32_t elapsed_us(uint32_t start_cycles)
{
	uint32_t delta_cycles = k_cycle_get_32() - start_cycles;
	uint64_t elapsed = k_cyc_to_us_floor64(delta_cycles);

	return elapsed == 0U ? 1U : (uint32_t)MIN(elapsed, (uint64_t)UINT32_MAX);
}

static uint32_t bytes_per_second(uint32_t bytes, uint32_t elapsed_us_value)
{
	uint64_t rate = ((uint64_t)bytes * 1000000ULL) / elapsed_us_value;

	return (uint32_t)MIN(rate, (uint64_t)UINT32_MAX);
}

static uint32_t random_block(uint32_t index)
{
	return (index * W25Q64_RANDOM_STEP) & (W25Q64_RANDOM_BLOCKS - 1U);
}

static void fill_pattern(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(write_buf); i++) {
		write_buf[i] = (uint8_t)(0x5aU ^ (uint8_t)i ^ (uint8_t)(i >> 3));
	}
}

static int fail(int step, int ret)
{
	w25q64_benchmark_result.magic = W25Q64_FAIL_MAGIC;
	w25q64_benchmark_result.step = step;
	w25q64_benchmark_result.ret = ret;
	printk("W25Q64 benchmark FAIL step=%d ret=%d\n", step, ret);

	return ret == 0 ? -EIO : ret;
}

static int check_equal(int step, const uint8_t *expected, const uint8_t *actual, size_t len,
		       uint32_t base_index)
{
	for (size_t i = 0; i < len; i++) {
		if (actual[i] != expected[i]) {
			w25q64_benchmark_result.mismatch_index = base_index + (uint32_t)i;
			w25q64_benchmark_result.expected = expected[i];
			w25q64_benchmark_result.actual = actual[i];
			return fail(step, -EIO);
		}
	}

	return 0;
}

static int check_erased(const struct device *flash, int step)
{
	int ret = flash_read(flash, W25Q64_TEST_OFFSET, read_buf, sizeof(read_buf));

	if (ret != 0) {
		return fail(step, ret);
	}

	for (size_t i = 0; i < ARRAY_SIZE(read_buf); i++) {
		if (read_buf[i] != W25Q64_ERASE_VALUE) {
			w25q64_benchmark_result.mismatch_index = (uint32_t)i;
			w25q64_benchmark_result.expected = W25Q64_ERASE_VALUE;
			w25q64_benchmark_result.actual = read_buf[i];
			return fail(step, -EIO);
		}
	}

	return 0;
}

static int erase_test_area(const struct device *flash, int step, uint32_t *elapsed)
{
	uint32_t start = k_cycle_get_32();
	int ret = flash_erase(flash, W25Q64_TEST_OFFSET, W25Q64_TEST_SIZE);

	*elapsed = elapsed_us(start);
	if (ret != 0) {
		return fail(step, ret);
	}

	return 0;
}

static int verify_random_layout(const struct device *flash, int step)
{
	for (uint32_t i = 0; i < W25Q64_RANDOM_BLOCKS; i++) {
		uint32_t block = random_block(i);
		uint32_t offset = block * W25Q64_RANDOM_CHUNK;
		int ret = flash_read(flash, W25Q64_TEST_OFFSET + offset, random_chunk,
				     sizeof(random_chunk));

		if (ret != 0) {
			return fail(step, ret);
		}

		ret = check_equal(step, &write_buf[offset], random_chunk, sizeof(random_chunk), offset);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

int main(void)
{
	const struct device *flash = DEVICE_DT_GET(W25Q64_NODE);
	const struct flash_parameters *params;
	struct flash_pages_info page_info;
	uint32_t start;
	uint64_t size;
	int ret;

	memset((void *)&w25q64_benchmark_result, 0, sizeof(w25q64_benchmark_result));
	w25q64_benchmark_result.step = 1;
	w25q64_benchmark_result.test_offset = W25Q64_TEST_OFFSET;
	w25q64_benchmark_result.test_size = W25Q64_TEST_SIZE;
	w25q64_benchmark_result.chunk_size = W25Q64_RANDOM_CHUNK;
	fill_pattern();

	if (!device_is_ready(flash)) {
		(void)fail(1, -ENODEV);
		goto done;
	}

	ret = flash_get_size(flash, &size);
	if (ret != 0) {
		(void)fail(2, ret);
		goto done;
	}
	w25q64_benchmark_result.flash_size = size;
	if (size != W25Q64_SIZE) {
		(void)fail(3, -EINVAL);
		goto done;
	}

	params = flash_get_parameters(flash);
	if ((params == NULL) || (params->write_block_size != 1U) ||
	    (params->erase_value != W25Q64_ERASE_VALUE)) {
		(void)fail(4, -EINVAL);
		goto done;
	}

	w25q64_benchmark_result.page_count = flash_get_page_count(flash);
	if (w25q64_benchmark_result.page_count != (W25Q64_SIZE / W25Q64_SECTOR_SIZE)) {
		(void)fail(5, -EINVAL);
		goto done;
	}

	ret = flash_get_page_info_by_offs(flash, W25Q64_TEST_OFFSET, &page_info);
	if (ret != 0) {
		(void)fail(6, ret);
		goto done;
	}
	if ((page_info.start_offset != W25Q64_TEST_OFFSET) || (page_info.size != W25Q64_SECTOR_SIZE)) {
		(void)fail(7, -EINVAL);
		goto done;
	}

	ret = flash_read_jedec_id(flash, (uint8_t *)w25q64_benchmark_result.jedec_id);
	if (ret != 0) {
		(void)fail(8, ret);
		goto done;
	}
	if ((w25q64_benchmark_result.jedec_id[0] != 0xefU) ||
	    (w25q64_benchmark_result.jedec_id[1] != 0x40U) ||
	    (w25q64_benchmark_result.jedec_id[2] != 0x17U)) {
		(void)fail(9, -EINVAL);
		goto done;
	}

	ret = flash_sfdp_read(flash, 0, (uint8_t *)w25q64_benchmark_result.sfdp_header,
			       sizeof(w25q64_benchmark_result.sfdp_header));
	if (ret != 0) {
		(void)fail(10, ret);
		goto done;
	}
	if ((w25q64_benchmark_result.sfdp_header[0] != 'S') ||
	    (w25q64_benchmark_result.sfdp_header[1] != 'F') ||
	    (w25q64_benchmark_result.sfdp_header[2] != 'D') ||
	    (w25q64_benchmark_result.sfdp_header[3] != 'P')) {
		(void)fail(11, -EINVAL);
		goto done;
	}

	ret = flash_ex_op(flash, FLASH_EX_OP_RESET, 0, NULL);
	if (ret != 0) {
		(void)fail(12, ret);
		goto done;
	}

	ret = flash_erase(flash, W25Q64_TEST_OFFSET + 1U, W25Q64_SECTOR_SIZE);
	if (ret == 0) {
		(void)fail(13, -EIO);
		goto done;
	}

	ret = erase_test_area(flash, 14, (uint32_t *)&w25q64_benchmark_result.erase_us);
	if (ret != 0) {
		goto done;
	}
	if (check_erased(flash, 15) != 0) {
		goto done;
	}

	start = k_cycle_get_32();
	ret = flash_write(flash, W25Q64_TEST_OFFSET, write_buf, sizeof(write_buf));
	w25q64_benchmark_result.sequential_write_us = elapsed_us(start);
	w25q64_benchmark_result.sequential_write_Bps =
		bytes_per_second(sizeof(write_buf), w25q64_benchmark_result.sequential_write_us);
	if (ret != 0) {
		(void)fail(16, ret);
		goto cleanup;
	}

	start = k_cycle_get_32();
	ret = flash_read(flash, W25Q64_TEST_OFFSET, read_buf, sizeof(read_buf));
	w25q64_benchmark_result.sequential_read_us = elapsed_us(start);
	w25q64_benchmark_result.sequential_read_Bps =
		bytes_per_second(sizeof(read_buf), w25q64_benchmark_result.sequential_read_us);
	if (ret != 0) {
		(void)fail(17, ret);
		goto cleanup;
	}
	if (check_equal(18, write_buf, read_buf, sizeof(write_buf), 0U) != 0) {
		goto cleanup;
	}

	ret = erase_test_area(flash, 19, (uint32_t *)&w25q64_benchmark_result.cleanup_erase_us);
	if (ret != 0) {
		goto done;
	}

	start = k_cycle_get_32();
	for (uint32_t i = 0; i < W25Q64_RANDOM_BLOCKS; i++) {
		uint32_t block = random_block(i);
		uint32_t offset = block * W25Q64_RANDOM_CHUNK;

		ret = flash_write(flash, W25Q64_TEST_OFFSET + offset, &write_buf[offset],
				  W25Q64_RANDOM_CHUNK);
		if (ret != 0) {
			w25q64_benchmark_result.random_write_us = elapsed_us(start);
			(void)fail(20, ret);
			goto cleanup;
		}
	}
	w25q64_benchmark_result.random_write_us = elapsed_us(start);
	w25q64_benchmark_result.random_write_Bps =
		bytes_per_second(sizeof(write_buf), w25q64_benchmark_result.random_write_us);

	start = k_cycle_get_32();
	for (uint32_t i = 0; i < W25Q64_RANDOM_BLOCKS; i++) {
		uint32_t block = random_block(i);
		uint32_t offset = block * W25Q64_RANDOM_CHUNK;

		ret = flash_read(flash, W25Q64_TEST_OFFSET + offset, random_chunk,
				 W25Q64_RANDOM_CHUNK);
		if (ret != 0) {
			w25q64_benchmark_result.random_read_us = elapsed_us(start);
			(void)fail(21, ret);
			goto cleanup;
		}
	}
	w25q64_benchmark_result.random_read_us = elapsed_us(start);
	w25q64_benchmark_result.random_read_Bps =
		bytes_per_second(sizeof(write_buf), w25q64_benchmark_result.random_read_us);

	if (verify_random_layout(flash, 22) != 0) {
		goto cleanup;
	}

cleanup:
	ret = erase_test_area(flash, 23, (uint32_t *)&w25q64_benchmark_result.cleanup_erase_us);
	if (ret != 0) {
		goto done;
	}
	if (check_erased(flash, 24) != 0) {
		goto done;
	}

	w25q64_benchmark_result.step = 25;
	w25q64_benchmark_result.ret = 0;
	w25q64_benchmark_result.magic = W25Q64_PASS_MAGIC;

	printk("W25Q64 benchmark PASS\n");
	printk("JEDEC %02x %02x %02x SFDP %02x %02x %02x %02x\n",
	       w25q64_benchmark_result.jedec_id[0], w25q64_benchmark_result.jedec_id[1],
	       w25q64_benchmark_result.jedec_id[2], w25q64_benchmark_result.sfdp_header[0],
	       w25q64_benchmark_result.sfdp_header[1], w25q64_benchmark_result.sfdp_header[2],
	       w25q64_benchmark_result.sfdp_header[3]);
	printk("erase=%u us seq_write=%u us seq_read=%u us random_write=%u us random_read=%u us\n",
	       w25q64_benchmark_result.erase_us, w25q64_benchmark_result.sequential_write_us,
	       w25q64_benchmark_result.sequential_read_us, w25q64_benchmark_result.random_write_us,
	       w25q64_benchmark_result.random_read_us);
	printk("seq_write=%u B/s seq_read=%u B/s random_write=%u B/s random_read=%u B/s\n",
	       w25q64_benchmark_result.sequential_write_Bps,
	       w25q64_benchmark_result.sequential_read_Bps,
	       w25q64_benchmark_result.random_write_Bps,
	       w25q64_benchmark_result.random_read_Bps);

done:
	while (true) {
		k_sleep(K_SECONDS(1));
	}
}
