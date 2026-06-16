# W25Q64 Flash API Benchmark

Date: 2026-06-16
Status: Runtime Verified

## Scope

This report covers the Titan Board Mini onboard Winbond W25Q64 running through the repo-local Zephyr flash driver at `src/drivers/flash/flash_titan_w25q64_ra_ospi_b.c`.

The benchmark app is `src/test/benchmark.c`. It uses Zephyr standard flash APIs only:

- `DEVICE_DT_GET(DT_NODELABEL(w25q64))`
- `device_is_ready()`
- `flash_get_size()`
- `flash_get_parameters()`
- `flash_get_page_count()`
- `flash_get_page_info_by_offs()`
- `flash_read_jedec_id()`
- `flash_sfdp_read()`
- `flash_ex_op(..., FLASH_EX_OP_RESET, ...)`
- `flash_erase()`
- `flash_write()`
- `flash_read()`

The test is destructive only within the last 16 KiB of W25Q64: offset `0x7fc000`, size `0x4000`. The app cleanup-erases that region and verifies it reads back as `0xff` before reporting PASS.

## Hardware and Software

- Board: `titan_board_mini/r7ka8p1kflcac/cm85`
- Target: `R7KA8P1KF_CPU0`
- Probe: J-Link over SWD
- J-Link version during flash/debug: 9.50
- Zephyr: 4.4.0 at `/home/lostdeers/zephyrproject/zephyr`
- Toolchain: `/home/lostdeers/zephyr-sdk-1.0.1`
- Worktree: `/home/lostdeers/projects/titan-board-mini-w25q64`
- Build directory: `build/w25q64-benchmark`

## Build and Flash

Build command:

```sh
/home/lostdeers/zephyrproject/.venv/bin/west build -p always \
  -b titan_board_mini/r7ka8p1kflcac/cm85 \
  /home/lostdeers/projects/titan-board-mini-w25q64/src/test \
  -d /home/lostdeers/projects/titan-board-mini-w25q64/build/w25q64-benchmark \
  -- -DZEPHYR_EXTRA_MODULES=/home/lostdeers/projects/titan-board-mini-w25q64
```

Build result: PASS.

Memory report:

- FLASH: 65,156 B / 768 KiB, 8.29%
- RAM: 46,152 B / 1 MiB, 4.40%
- SDRAM: 0 B / 64 MiB

Flash command:

```sh
/home/lostdeers/zephyrproject/.venv/bin/west flash \
  -d /home/lostdeers/projects/titan-board-mini-w25q64/build/w25q64-benchmark
```

Flash result: PASS.

Observed runner output:

- `west flash: using runner jlink`
- `JLink version: 9.50`
- `Flashing file: .../build/w25q64-benchmark/zephyr/zephyr.hex`

## Generated Configuration Checks

`build/w25q64-benchmark/zephyr/.config` contains:

- `CONFIG_TITAN_W25Q64_RA_OSPI_B=y`
- `CONFIG_FLASH_JESD216=y`
- `CONFIG_FLASH_JESD216_API=y`
- `CONFIG_FLASH_PAGE_LAYOUT=y`
- `CONFIG_FLASH_EX_OP_ENABLED=y`

`FLASH_RENESAS_RA_OSPI_B` is not present, so Zephyr did not select the upstream RA OSPI_B flash driver.

`build/w25q64-benchmark/zephyr/zephyr.dts` contains:

- `w25q64: flash@90000000`
- `compatible = "titan,w25q64-ra-ospi-b"`
- `reg = <0x90000000 0x800000>`
- `jedec-id = [ EF 40 17 ]`
- `page-size = <0x100>`
- `erase-block-size = <0x1000>`
- `write-block-size = <0x1>`

`build/w25q64-benchmark/build.ninja` contains:

- `src/drivers/flash/flash_titan_w25q64_ra_ospi_b.c.obj`
- Renesas HAL `r_ospi_b.c.obj`

No `flash_renesas_ra_ospi_b.c.obj` entry was selected.

## Runtime Collection

Runtime result was collected through J-Link GDB server and `arm-zephyr-eabi-gdb`.

GDB command:

```sh
/home/lostdeers/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb \
  -q /home/lostdeers/projects/titan-board-mini-w25q64/build/w25q64-benchmark/zephyr/zephyr.elf \
  -ex "target remote :2331" \
  -ex "monitor halt" \
  -ex "p w25q64_benchmark_result" \
  -ex "p/x w25q64_benchmark_result" \
  -ex "detach" \
  -ex "quit"
```

Observed PASS state:

```text
magic = 0x57324250
step = 0x19
ret = 0x0
jedec_id = {0xef, 0x40, 0x17}
sfdp_header = {0x53, 0x46, 0x44, 0x50, 0x5, 0x1, 0x0, 0xff}
flash_size = 0x800000
page_count = 0x800
test_offset = 0x7fc000
test_size = 0x4000
chunk_size = 0x20
mismatch_index = 0x0
expected = 0x0
actual = 0x0
```

Functional checks passed:

- `w25q64` device ready.
- Flash size equals 8 MiB.
- Flash parameters report `write_block_size = 1` and `erase_value = 0xff`.
- Page layout reports 2048 erase pages; page at `0x7fc000` has size `0x1000`.
- JEDEC ID equals `ef 40 17`.
- SFDP header begins with `SFDP`.
- `FLASH_EX_OP_RESET` succeeds.
- Unaligned erase at `0x7fc001` is rejected.
- 16 KiB test area erase succeeds and reads back as all `0xff`.
- Sequential 16 KiB write/readback compare passes.
- Random 32-byte chunk write/readback over the same 16 KiB area passes.
- Cleanup erase succeeds and reads back as all `0xff`.

## Benchmark Method

Timing uses `k_cycle_get_32()` and `k_cyc_to_us_floor64()` inside the target firmware.

Test area:

- Offset: `0x7fc000`
- Size: 16 KiB
- Erase coverage: 4 sectors × 4 KiB

Sequential write:

- One `flash_write()` call over the full 16 KiB test area.
- Driver internally chunks at the W25Q64 page boundary and RA OSPI_B direct-transfer limit.

Sequential read:

- One `flash_read()` call over the full 16 KiB test area.

Random write:

- 512 writes × 32 B each.
- Offset order is a deterministic full permutation: `(i × 73) & 511`, scaled by 32 bytes.
- The area is erased before random writes.

Random read:

- 512 reads × 32 B each.
- Same deterministic full-permutation order as random write.

## Benchmark Results

| Operation | Bytes | Time | Rate |
| --- | ---: | ---: | ---: |
| Erase 16 KiB test area | 16,384 | 238,399 us | n/a |
| Sequential write | 16,384 | 409,540 us | 40,005 B/s |
| Sequential read | 16,384 | 16,548 us | 990,089 B/s |
| Random write, 512 × 32 B | 16,384 | 409,599 us | 40,000 B/s |
| Random read, 512 × 32 B | 16,384 | 16,917 us | 968,493 B/s |
| Cleanup erase 16 KiB test area | 16,384 | 244,571 us | n/a |

## Interpretation

The read path is near 1 MB/s for both sequential and 32-byte random reads. Random reads are about 2.2% slower than sequential reads in this benchmark.

The write path is about 40 KB/s for both sequential and 32-byte random writes. That matches the first-version driver design: writes are correctness-first, 1S-1S-1S, and internally limited by RA OSPI_B direct-transfer chunks and W25Q64 program polling.

These numbers are a baseline for the conservative first-version driver. They are not a Quad, XIP, DMA, cache-optimized, or memory-mapped read ceiling.

## Warning Triage

Accepted as existing/unrelated to W25Q64 benchmark behavior:

- Devicetree warning: `/soc/trng` missing or empty `reg`/`ranges` property.
- Devicetree warning: `/soc/eswm@403c8000/ethernet_mac@403cd000` duplicate unit address with `/soc/eswm@403c8000/mdio@403cd000`.

USB/network warning noise is absent because `src/test/prj.conf` disables USB device stack and networking.

## Residual Risks

- Benchmark covers only conservative 1S-1S-1S mode.
- Quad mode is not implemented or verified.
- Test area is the last 16 KiB of W25Q64. Reserve a formal scratch/test partition before routine destructive test use.
- Filesystems and partition-manager consumers are not covered here; this app validates raw Zephyr flash API behavior.
