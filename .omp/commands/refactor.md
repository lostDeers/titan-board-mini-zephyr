---
description: Execute a behavior-preserving refactor with the required verification gate
---
# Refactor

Execute a behavior-preserving refactor for: $ARGUMENTS

## Required Invocation

Use for mechanical or small internal refactors when the verification path is clear, or for larger refactors after `/refactor-plan` approval.

If the requested change affects public API, architecture, data model, framework choice, module boundaries, hardware abstraction boundaries, concurrency ownership, or hardware-sensitive paths and approval is not already present, stop and produce a `/refactor-plan` result instead of editing.

## Workflow

1. Read `AGENTS.md`, `.omp/instructions/INSTRUCTIONS.md`, `.omp/instructions/TASK_ROUTING_MATRIX.md`, and `.omp/instructions/REFACTORING_STANDARD.md`.
2. Classify the refactor and select the strictest applicable route.
3. Identify behavior invariants, public/exported symbols, callsites, and ABI/layout/protocol/config contracts.
4. Establish baseline verification when feasible.
5. Apply a clean cutover; avoid stale aliases, wrappers, duplicate paths, and unrelated formatting churn.
6. Update directly affected tests, callsites, docs, scripts, and project-local references.
7. Run `/host-test` for host-testable logic when feasible.
8. Build and inspect generated `.config`, `zephyr.dts`, generated headers, map, or ELF symbols when generated state matters.
9. Run `/embedded-review` for meaningful embedded source, DTS/Kconfig/CMake, scripts, docs/evidence, or reference-policy changes.
10. Run `/reference-check` for refactors touching `references/`, copied vendor/reference code, provenance docs, active build paths, CMake/source includes, or `memory/references.md`.
11. Flash/runtime verify and `/record-evidence` for hardware-sensitive refactors or when hardware behavior can be affected.

## Output

Report:

- Refactor class.
- Files changed.
- Behavior invariants preserved.
- Public contracts and callsites checked.
- Verification commands and PASS/FAIL.
- Review result.
- Hardware/runtime evidence status from `.omp/instructions/STATUS_AND_EVIDENCE.md` when applicable.
- Remaining risks or verification gaps.
