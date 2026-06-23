---
description: Diagnose hardware runtime behavior on a Zephyr target
---
# Hardware Debug

Diagnose this hardware/runtime issue: $ARGUMENTS

## Required Invocation

Use when a task needs target-side evidence or Runtime Verified status, including:
- Flash/runtime failures, boot failures, hangs, faults, or missing logs.
- Probe, J-Link, RTT, UART, USB CDC ACM, semihosting, or monitor issues.
- IRQ, DMA, cache, NPU, display, audio, storage, network, sensor, or bus behavior.
- Physical/visible behavior: display, LED, audio, actuator, relay, touch, motion, measurement.
- Any claim that hardware behavior is fixed or Runtime Verified.

Do not use build success alone to validate runtime hardware behavior.
Use `.omp/instructions/STATUS_AND_EVIDENCE.md` for status classification and `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md` for streaming, DMA, cache, or producer/consumer paths.

## Workflow

If the task tool is available, delegate to the `hardware-debugger` agent. Confirm app, board, core, build dir, probe, console, and artifact before running target commands. Gather exact evidence and classify the status.

## Output

Report:
- Current hypothesis.
- Target context and artifact.
- Commands run.
- Evidence collected.
- Findings or root cause.
- Next minimal diagnostic or fix.
- Completion status from `.omp/instructions/STATUS_AND_EVIDENCE.md`: Blocked, Built, Flashed, Runtime Verified, or Visual Pending.
