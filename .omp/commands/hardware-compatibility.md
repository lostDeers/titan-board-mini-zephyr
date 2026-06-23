---
description: Explore hardware compatibility and feasibility before bring-up
---
# Hardware Compatibility Exploration

Explore hardware compatibility and bring-up feasibility for: $ARGUMENTS

This is a research task, not a build/flash/debug task by default.
Use `.omp/templates/hardware-compatibility-report.md` for report structure when useful.

## Required Invocation

Use before implementing support for a new display, sensor, radio, storage device, shield, connector, SoC peripheral, or hardware module when feasibility, wiring, driver support, or interface choice is not already established in project memory/evidence.

Do not use after implementation has started as a substitute for build/flash/runtime validation.

## Scope

- Identify the target part/module, intended interface, board connector or SoC peripheral, and expected use case.
- Check existing upstream Zephyr support, bindings, Kconfig symbols, SoC HAL support, and known driver limitations.
- Compare board wiring against available references: schematic, board DTS, pinctrl, reference BSP, datasheet, examples, and project memory.
- Identify power rails, voltage levels, reset/enable pins, IRQ lines, bus addresses, clock requirements, DMA/cache/alignment constraints, and shared-pin conflicts.
- Prefer existing drivers or a small board-specific shim over custom driver work.

## Default Boundaries

- Do not write code, edit DTS/Kconfig, create apps, or add scripts unless the user explicitly asks for implementation.
- Do not run build, flash, RTT/UART monitor, GDB, logic-analyzer, or target-debug commands unless the user explicitly asks for validation after the feasibility report.
- Do not mark hardware behavior as Implemented, Built, Flashed, Runtime Verified, Visual Pending, or complete.
- Do not create `memory/evidence/` records unless real build/flash/runtime evidence is produced in a separate validation step.

## Allowed Actions

- Read project instructions, memory, docs, references, DTS/Kconfig/source, bindings, and driver code.
- Search the local Zephyr tree and reference material.
- Fetch public documentation when needed and allowed.
- Record durable facts or non-obvious pitfalls if project policy requires it, clearly labeled as research findings rather than verified runtime evidence. If the user explicitly forbids modifications, report the candidate facts instead.

## Output

Return a concise feasibility report with:

1. Compatibility verdict: supported, likely supported, possible with small changes, custom driver likely, blocked, or unknown.
2. Evidence: exact files, bindings, symbols, reference paths, datasheet sections, or observations used.
3. Integration sketch: expected driver/binding, DTS/Kconfig shape, bus/pins/power/reset/IRQ needs, and minimal app or probe strategy if later requested.
4. Risks and unknowns: hardware facts that still need schematic, measurement, ID read, address probe, or vendor documentation.
5. Next validation step: the smallest useful build or hardware check, but only as a recommendation unless the user asks to execute it.
