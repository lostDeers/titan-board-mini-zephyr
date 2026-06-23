---
description: Record build/flash/runtime evidence into memory/evidence or current task notes
---
# Record Evidence

Create or update an evidence/status record for: $ARGUMENTS

## Required Invocation

Use after any build, generated config/DTS/ELF/map inspection, flash, monitor, debug, runtime, visual, or measured observation that will be cited in a final response, memory file, or completion claim.
Use `.omp/instructions/STATUS_AND_EVIDENCE.md` as the canonical status vocabulary and evidence schema. If the user explicitly forbids modifications, do not update files; return an evidence record candidate instead.

Use even when the gate is incomplete:
- Build passed but target was not flashed -> status `Built`, with runtime evidence missing.
- Flash passed but runtime was not checked -> status `Flashed`, with runtime evidence missing.
- Runtime logs passed but visible/physical behavior was not observed -> Visual Pending.
- Hardware/probe/access is unavailable -> Blocked with exact blocker.

## Workflow

If the task tool is available, delegate to the `evidence-recorder` agent. Summarize generated config, DTS, ELF/map, build, flash, and runtime observations. Use `.omp/templates/evidence.md` for structure. Do not commit generated build outputs.

## Output

Record or report:
- Context: app, board, core, build dir, toolchain, probe/console, connected hardware.
- Build command, PASS/FAIL, artifact path, FLASH/RAM usage, warnings.
- Relevant `.config`, `zephyr.dts`, generated headers, map/ELF symbols when behavior depends on them.
- Flash command, runner, artifact, Program/Verify result.
- Runtime logs/registers/counters/visual or measured observations.
- Warning triage.
- Final status using `.omp/instructions/STATUS_AND_EVIDENCE.md`: Implemented, Built, Flashed, Runtime Verified, Visual Pending, Blocked, or Abandoned.
