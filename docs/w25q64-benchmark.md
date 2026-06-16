# W25Q64 Quad Memory-Mapped Read and XIP Benchmark

Date: 2026-06-16
Status: Runtime Verified for Quad I/O memory-mapped reads and minimal instruction-fetch XIP; Quad program/write remains unimplemented

## Scope

This report covers the Titan Board Mini onboard Winbond W25Q64 running through the repo-local Zephyr flash driver at `src/drivers/flash/flash_titan_w25q64_ra_ospi_b.c` after adding volatile QE-bit setup, a Runtime Verified Quad I/O memory-mapped read path, and a minimal instruction-fetch XIP probe.

The benchmark app is `src/test/benchmark.c`. It uses Zephyr standard flash APIs only; Quad memory-mapped read is reached through the driver's `flash_read()` implementation:

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
- Build directory: `build/w25q64-quad-read`

## Build and Flash

Build command:

```sh
/home/lostdeers/zephyrproject/.venv/bin/west build -p always \
  -b titan_board_mini/r7ka8p1kflcac/cm85 \
  /home/lostdeers/projects/titan-board-mini-w25q64/src/test \
  -d /home/lostdeers/projects/titan-board-mini-w25q64/build/w25q64-quad-read \
  -- -DZEPHYR_EXTRA_MODULES=/home/lostdeers/projects/titan-board-mini-w25q64
```

Build result: PASS.

Memory report:

- FLASH: 65,676 B / 768 KiB, 8.35%
- RAM: 46,152 B / 1 MiB, 4.40%
- SDRAM: 0 B / 64 MiB

Flash command:

```sh
/home/lostdeers/zephyrproject/.venv/bin/west flash \
  -d /home/lostdeers/projects/titan-board-mini-w25q64/build/w25q64-quad-read
```

Flash result: PASS.

Observed runner output:

- `west flash: using runner jlink`
- `JLink version: 9.50`
- `Flashing file: .../build/w25q64-quad-read/zephyr/zephyr.hex`

## Generated Configuration Checks

`build/w25q64-quad-read/zephyr/.config` contains:

- `CONFIG_TITAN_W25Q64_RA_OSPI_B=y`
- `CONFIG_FLASH_JESD216=y`
- `CONFIG_FLASH_JESD216_API=y`
- `CONFIG_FLASH_PAGE_LAYOUT=y`
- `CONFIG_FLASH_EX_OP_ENABLED=y`

Cache note: `CONFIG_CACHE_MANAGEMENT` and `CONFIG_DCACHE` are not selected in this build, so the benchmark timings do not include data-cache maintenance effects.

`FLASH_RENESAS_RA_OSPI_B` is not present, so Zephyr did not select the upstream RA OSPI_B flash driver.

`build/w25q64-quad-read/zephyr/zephyr.dts` contains:

- `w25q64: flash@90000000`
- `compatible = "titan,w25q64-ra-ospi-b"`
- `reg = <0x90000000 0x800000>`
- `jedec-id = [ EF 40 17 ]`
- `page-size = <0x100>`
- `erase-block-size = <0x1000>`
- `write-block-size = <0x1>`

`build/w25q64-quad-read/build.ninja` contains:

- `src/drivers/flash/flash_titan_w25q64_ra_ospi_b.c.obj`
- Renesas HAL `r_ospi_b.c.obj`

No `flash_renesas_ra_ospi_b.c.obj` entry was selected.

## Runtime Collection

Runtime result was collected through J-Link GDB server and `arm-zephyr-eabi-gdb`.

GDB command:

```sh
/home/lostdeers/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb \
  -q /home/lostdeers/projects/titan-board-mini-w25q64/build/w25q64-quad-read/zephyr/zephyr.elf \
  -ex "target remote :2331" \
  -ex "monitor halt" \
  -ex "p/x w25q64_benchmark_result" \
  -ex "p/x titan_w25q64_data.bus_mode" \
  -ex "p/x titan_w25q64_data.ctrl.spi_protocol" \
  -ex "p/x titan_w25q64_data.ctrl.p_reg->CMCFGCS[titan_w25q64_data.ctrl.channel].CMCFG0" \
  -ex "p/x titan_w25q64_data.ctrl.p_reg->CMCFGCS[titan_w25q64_data.ctrl.channel].CMCFG1" \
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

Driver register/state observation after the benchmark:

```text
titan_w25q64_data.bus_mode = 0x1
titan_w25q64_data.ctrl.spi_protocol = 0x90
CMCFG0 = 0xfff00008
CMCFG1 = 0x0006eb00
```

`0x90` is Renesas FSP `SPI_FLASH_PROTOCOL_1S_4S_4S`, used for W25Q64 `0xeb` Quad I/O memory-mapped reads. `CMCFG0.ADDRPCD = 0xff` supplies the W25Q64 mode byte, and `CMCFG1.RDLATE = 6` is the verified latency for this board at the observed 33.3 MHz OSPI clock. Writes and erase commands still switch back to `SPI_FLASH_PROTOCOL_EXTENDED_SPI` and use conservative 1S-1S-1S W25Q64 commands.

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
- `flash_read()` runs through the Quad I/O memory-mapped path and completed the sequential and random readback checks.

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
| Erase 16 KiB test area | 16,384 | 257,794 us | n/a |
| Sequential write | 16,384 | 409,554 us | 40,004 B/s |
| Sequential read, Quad I/O memory-mapped | 16,384 | 2,555 us | 6,412,524 B/s |
| Random write, 512 × 32 B | 16,384 | 409,599 us | 40,000 B/s |
| Random read, 512 × 32 B, Quad I/O memory-mapped | 16,384 | 5,653 us | 2,898,284 B/s |
| Cleanup erase 16 KiB test area | 16,384 | 257,807 us | n/a |

## Interpretation

Quad I/O memory-mapped reads are Runtime Verified. Sequential read improved from the previous conservative 1S read baseline of 990,089 B/s to 6,412,524 B/s. Random 32-byte reads improved from 968,493 B/s to 2,898,284 B/s.

Root cause of the earlier memory-mapped data-read failure: the memory-map path used W25Q64 `0xeb` with `RDLATE = 12`, treating the required 8-bit mode byte as dummy clocks. The flash therefore never saw the recommended `0xff`/Fxh mode byte before data. The fixed configuration programs `CMCFG0.ADDRPCD = 0xff` and uses `RDLATE = 6`; the probe app found that exact memory-map configuration matched data written through the flash API.

Minimal instruction-fetch XIP was verified with a temporary probe app at `.tmp/xip_exec_probe`. The probe wrote two 4-byte Thumb functions into W25Q64 at `0x907fc000` and `0x907fc100`, verified flash API readback and raw memory-window bytes, invalidated the instruction cache range, then called the functions through Thumb addresses `0x907fc001` and `0x907fc101`. Runtime result: `magic = 0x58495045`, `step = 0x11`, `ret = 0`, `func0_result = 0x5a`, `func1_result = 0xc3`, `CMCFG0 = 0xfff00008`, `CMCFG1 = 0x0006eb00`, `LIOCFGCS = 0x00010090`.

The write path remains about 40 KB/s because it intentionally stays on the Runtime Verified 1S-1S-1S W25Q64 Page Program `0x02` path. The Renesas RA OSPI_B HAL path available here does not safely express W25Q64 Quad Input Page Program `0x32` as a 1-1-4 transaction.

## Warning Triage

Accepted as existing/unrelated to W25Q64 benchmark behavior:

- Devicetree warning: `/soc/trng` missing or empty `reg`/`ranges` property.
- Devicetree warning: `/soc/eswm@403c8000/ethernet_mac@403cd000` duplicate unit address with `/soc/eswm@403c8000/mdio@403cd000`.

USB/network warning noise is absent because `src/test/prj.conf` disables USB device stack and networking.

## Residual Risks

- Quad program is not implemented; writes remain conservative 1S-1S-1S page program.
- Minimal instruction-fetch XIP is verified for simple Thumb functions written to W25Q64 and called through the OSPI_B memory window. Full linker-placed code execution from W25Q64 is not covered by this benchmark.
- Full-XIP code must not call flash API operations that reconfigure OSPI_B out of Quad memory-map mode while executing from W25Q64; that integration pattern is not covered here.
- Test area is the last 16 KiB of W25Q64. Reserve a formal scratch/test partition before routine destructive test use.
- Filesystems and partition-manager consumers are not covered here; this app validates raw Zephyr flash API behavior.
