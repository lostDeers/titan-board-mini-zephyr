---
description: Run or create the narrowest host/unit test for host-testable embedded logic
---
# Host Test

Run or create host/unit tests for: $ARGUMENTS

## Required Invocation

Use this command when changes touch pure software logic that can be exercised without target hardware, including:
- Protocol encoders/decoders and parsers.
- Shared-memory, mailbox, packet, register, or ABI/layout helpers.
- Reusable host tools and scripts.
- Algorithms extracted from firmware paths.

Do not use host tests as a substitute for target verification when behavior depends on hardware, DeviceTree/Kconfig, IRQs, DMA/cache, timing, bus transactions, displays, sensors, audio, or flash/runtime behavior.

## Workflow

1. Read the changed files and identify the smallest host-testable boundary.
2. If a project host test runner exists, use it.
3. If no runner exists, create a minimal project-local runner under `scripts/` and tests under `tests/`.
4. Compile with strict warnings where practical, for example `-Wall -Wextra -Werror`.
5. Test logical behavior, layout/offset contracts, edge values, and error paths that can regress.
6. Run the test and report PASS/FAIL with the exact command.

## Output

Report:
- Test files added or used.
- Command run.
- PASS/FAIL.
- What behavior the test proves.
- What still requires build/flash/runtime verification.
