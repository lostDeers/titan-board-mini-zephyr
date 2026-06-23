# Streaming DMA Cache Checklist

Use this checklist for camera, audio, display refresh, storage, network, NPU, motor control, sensor streaming, and any producer/consumer firmware path.

## Design Before Coding

- Identify every producer and consumer.
- Identify producer-owned buffers, consumer-owned buffers, ownership transfer points, and release responsibilities.
- Define queue/ring full, empty, overwrite, drop-newest, drop-oldest, block, or fail-fast semantics explicitly.
- Prefer continuous capture/transfer with ring or double buffers over repeated stop/start loops when samples must be contiguous.
- Do not pass pointers to mutable producer/DMA buffers to slow consumers unless an ownership/release protocol prevents mutation until release.
- If payloads are copied to stable transport/storage buffers, record buffer count, ownership, and release path.
- Keep ISR work bounded and defer processing to threads/work queues.
- Throttle displays, logs, filesystem writes, host rendering, and diagnostics so they cannot starve capture or control loops.

## DMA / Cache / Memory

- Record buffer address, alignment, size, memory region, and bus-master access constraints.
- Confirm the DMA/NPU/network/display/audio/storage bus master can access the selected memory region.
- Keep producer/consumer metadata in memory that all relevant actors can safely access.
- Record cache enabled/disabled state and memory-controller state.
- Treat cache flush/invalidate boundaries and alignment as part of correctness proof.
- Inspect `.config`, `zephyr.dts`, map, and ELF symbols when placement or cache behavior matters.

## Diagnostics And Metrics

- Measure or log producer cadence, consumer cadence, buffer occupancy, overrun count, dropped-frame/sample count, retry/error counters, and recovery count.
- Separate fast-path metrics from validation-path metrics.
- Record whether metrics include full-buffer scans, CRCs, RTT/UART logs, image saves, host rendering/saving, display refresh, or filesystem writes.
- Prefer bounded logs such as first failure plus periodic counters over logging every loop failure.
- Record whether diagnostics read mutable producer memory, copied stable memory, or both.

## Runtime Evidence

- Baseline metric and build directory.
- Current metric from the same app path after the change.
- Generated `.config` evidence for cache, memory-controller, DMA/network/NPU/display/audio/storage, and relevant driver symbols.
- Map/ELF placement for DMA/NPU/network/display buffers and producer/consumer metadata.
- Data correctness evidence: CRC-valid frames, nonconstant buffers, monotonic samples, bounded dropped counters, or equivalent protocol checks.
- Recovery proof that data becomes fresh again after overflow/restart, not merely that error logs stop.
- Explicit residual risks: validation cadence, dropped policy, cache state, ownership assumptions, and unverified buffer paths.
