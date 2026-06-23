# Zephyr Debug Playbook

Use this playbook for Zephyr firmware bring-up, board debugging, drivers, and target-side runtime verification.

## Debug Probe / Console

- Identify the active probe and console backend before debugging: J-Link/RTT, pyOCD, OpenOCD, UART, USB CDC ACM, semihosting, simulator stdout, or another transport.
- Use project scripts when they exist; otherwise inspect runner configuration before inventing commands.

## J-Link / RTT

Use this section when the project uses SEGGER J-Link or SEGGER RTT.

- Confirm the target CPU name before connecting, especially on dual-core boards.
- Confirm GDB and RTT ports are not already owned by another debug session.
- Confirm RTT Kconfig symbols in `.config`: `CONFIG_USE_SEGGER_RTT`, `CONFIG_RTT_CONSOLE`, and log backend settings.
- If RTT connects but no application logs appear, do not assume the firmware is silent.
- Check `_SEGGER_RTT` with `nm` and RTT sections with `readelf -S`.
- If J-Link auto-search misses the control block, use `monitor exec SetRTTAddr = <addr>`.
- Capture runtime evidence from a fresh reset when possible.

### J-Link Runtime Counter Reads

When reading RAM counters after reset, make the target run before sampling. A plain `r` may leave the core halted, so `sleep` plus `mem32` can capture pre-runtime state.

Minimal command script pattern:

```text
connect
r
g
sleep <runtime_ms>
h
mem32 <counter_addr> <word_count>
exit
```

Evidence must record the counter symbol address and size, the script used, and the decoded fields. If the sampled `step` or counter state looks too early, rerun with a longer `sleep` before declaring a runtime failure.

## Connect Under Reset

- Use connect-under-reset when normal halt/load fails or running firmware blocks debug access.
- Record exact reset sequence and probe target.
- Flash evidence must include Program/Verify success, not only command completion.

## Dual-Core Boards

- Record which core owns the app, memory region, interrupts, and console.
- RTT control blocks are usually per-core, not shared.
- Do not use CPU0 logs to validate CPU1 behavior, or vice versa.
- Check core-specific SRAM/TCM regions before placing DMA, NPU, or shared buffers.

## Kconfig

- Inspect `build/zephyr/.config` after changing behavior through Kconfig.
- Record whether important symbols come from app `prj.conf`, board defconfig, SoC defaults, or dependencies.
- Do not enable drivers only to satisfy compilation if hardware or HAL support is incompatible.

## Devicetree

- Inspect `build/zephyr/zephyr.dts` when IRQs, clocks, pinctrl, aliases, chosen nodes, memory regions, or device readiness matter.
- Confirm unit addresses, memory sizes, interrupt names, and priorities match the active SoC include chain.
- DeviceTree describes hardware; do not encode runtime policy there.

## External Connectors And Buses

- Confirm physical connector pin, schematic net, SoC GPIO/pad, and peripheral function separately.
- Cross-check schematic, reference BSP pin config, and generated `zephyr.dts` before trusting a bus mapping.
- For I2C: verify idle-high, pull-ups, bus recovery behavior, address ACK/NACK, and shared-function conflicts before treating a probe as valid.
- For SPI: verify SCLK/MOSI/MISO/CS/control GPIO mapping, idle levels, chip-select polarity, and a low-speed first transaction before increasing speed.
- For UART: confirm voltage level and TX/RX crossover; do not assume 5 V tolerance.
- For PWM/GPIO/actuators: confirm current limits, external drivers, flyback protection, enable polarity, and fail-safe state.

## Display Bring-Up

- Separate bus success from visual success.
- For e-paper and other BUSY-driven displays, record reset sequence, BUSY timeout behavior, panel controller/model, refresh duration, and sleep/deep-sleep state.
- For frame-buffered displays, consider refresh bandwidth and whether full-screen updates can interfere with streaming capture or control loops.
- If logs pass but the visible result is not observed, report Visual Pending instead of complete.

## DMA / Cache / Alignment

- Record buffer address, alignment, size, memory region, and cache maintenance boundaries.
- Confirm the bus master can access the selected memory region.
- For NPU, display, audio, storage, network, and camera paths, treat cache flush/invalidate as part of correctness proof.
- If unaligned access traps, identify whether the issue is application data, HAL driver behavior, compiler assumptions, or CPU trap configuration.

## Streaming / Real-Time Debug
For streaming, DMA, cache, and producer/consumer evidence requirements, also use `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md`.

- Measure or log producer cadence, consumer cadence, buffer occupancy, overrun count, dropped-frame/sample count, and retry/error counters.
- Confirm slow diagnostics, RTT/UART logging, display refresh, filesystem writes, or inference do not starve capture/control paths.
- When optimizing performance, separate fast-path metrics from validation-path metrics. Full-buffer scans, CRCs, RTT/UART logs, image saves, host rendering, or other diagnostics can dominate latency; record their cadence and measured cost.
- Prefer bounded logs such as first failure plus periodic counters over logging every loop failure.
- Verify recovery paths by observing that data becomes fresh again after overflow/restart, not just that the error log stops.

## Interrupts

- Verify IRQ number, priority, route, pending bit, ISR entry, and clear sequence.
- For Renesas RA, check ICU/ELC/FSP event routing when internal peripheral events are involved.
- Avoid unusually high priorities unless justified; high-priority ISRs can interfere with SysTick or scheduler behavior.

## External Reference Code

- Treat `references/` as read-only documentation/source material.
- The canonical references policy is in `.omp/instructions/REFERENCES_POLICY.md`.
- Before copying reference code, confirm whether upstream support or a small shim can replace it.
- Copy only minimal required files or code blocks into active source directories.
- Preserve license headers and record the origin path in comments or docs.
- Keep copied code isolated behind project-local APIs to simplify future replacement.

## Runtime Evidence Examples

- Boot banner and app version.
- Device ID reads or ACK/NACK results.
- IRQ entry proof, counters, or pending/clear status.
- DMA/NPU/display/audio status registers.
- Frame/sample/packet counters.
- RTT/UART logs from a fresh reset.
- Visual or measured behavior when logs are insufficient.
