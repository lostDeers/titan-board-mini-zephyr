---
description: Resolve Zephyr build failures with minimal diffs
---
# Zephyr Build Fix

Resolve this Zephyr build failure with the smallest correct change: $ARGUMENTS

## Required Invocation

Use when a Zephyr build, CMake configure, Kconfig, Devicetree, generated header, linker, or memory-region failure blocks progress.

Do not use for target runtime/probe/hardware behavior failures; use `/hardware-debug` for those.

## Workflow

If the task tool is available, delegate to the `zephyr-build-resolver` agent. Use the project build command when documented. Read the full relevant build output, fix root cause with minimal diffs, rebuild, and report remaining warnings.

## Output

Report:
- Failing command.
- Root cause.
- Files changed.
- Verification command and PASS/FAIL.
- Remaining warnings and their classification.
