# Evidence Record Template

## Context

- Task:
- App:
- Board / qualifier:
- Core:
- Build dir:
- Toolchain / SDK:
- Probe:
- Console backend:
- Connected hardware revision:
- Git commit / base:
- Source state:
  - Clean / dirty:
  - Modified files:
  - Untracked files:
- External workspace state:
  - Zephyr base:
  - Module paths:
  - Local patches or uncommitted external changes:
  - Reproducibility risk:

## Build

- Command:
- Result:
- Artifact:
- FLASH / RAM usage:
- Warnings:

## Config

- Relevant `.config` symbols:
- Symbol sources:

## Devicetree

- Relevant `zephyr.dts` nodes:
- IRQs / clocks / pinctrl:
- Memory regions:
- Chosen nodes / aliases:

## ELF / Map

- Relevant symbols:
- Sections:
- Buffer addresses / alignment:
- ISR symbols:
- DMA/cache-sensitive placement:

## Flash

- Command:
- Runner:
- Artifact:
- Reset behavior:
- Program / Verify result:

## Runtime

- Logs:
- Register values:
- IDs / ACK/NACK / counters:
- Visual or measured behavior:
- Status:

## Performance / Streaming Addendum

- Baseline:
- Current metric:
- Measurement overhead:
- Data correctness:
- Buffer ownership:
- Cache / memory:

## Warning Triage

| Warning | Classification | Rationale |
| --- | --- | --- |

## Residual Risks

-
