# Hardware Compatibility Report Template

## Compatibility Verdict

Supported / likely supported / possible with small changes / custom driver likely / blocked / unknown.

## Target

- Part / module:
- Intended interface:
- Board connector or SoC peripheral:
- Expected use case:

## Evidence

- Zephyr drivers:
- Zephyr bindings:
- Kconfig symbols:
- SoC HAL support:
- Board schematic / DTS / pinctrl:
- Datasheet / reference BSP:
- Project memory:

## Integration Sketch

- Driver / binding:
- DTS shape:
- Kconfig shape:
- Bus / pins:
- Power rails and voltage levels:
- Reset / enable / IRQ:
- Address / chip select / clock:
- DMA / cache / alignment constraints:

## Risks And Unknowns

- Hardware facts still needing schematic confirmation:
- Measurements still needed:
- ID reads or address probes still needed:
- Vendor documentation gaps:
- Shared-pin or alternate-function conflicts:

## Smallest Later Validation Step

Describe the smallest useful build or hardware check. Do not execute it unless implementation or validation has been explicitly requested.

## Status Boundary

This report is research only. Do not mark hardware behavior Implemented, Built, Flashed, Runtime Verified, or complete from compatibility research alone.
