# Embedded Project Workflow Template

## Core Principles

- **Read Before Write**: understand project context before changing files.
- **Incremental Development**: complete one task at a time.
- **Truth > Speed**: never claim completion without verification evidence.
- **No Secrets**: never commit API keys, tokens, credentials, or private SDK paths.
- **Sanitized Docs Only**: unredacted personal data, local absolute paths, private SDK/toolchain paths, probe serials, and session artifacts are not git-maintained and do not belong in `docs/`; only sanitized code, debug, usage, and evidence content is maintained there.
- **Reversible**: prefer changes with a rollback path.
- **Self-verify**: run the narrowest useful verification before declaring done.
- **Banned phrases**: "I fixed it, you try" / "Should be fine" / "Probably passes" / "Theoretically correct" / "I think it's fixed".
- **Baseline required reads**: read `.omp/instructions/INSTRUCTIONS.md`, `.omp/instructions/TASK_ROUTING_MATRIX.md`, and `.omp/instructions/STATUS_AND_EVIDENCE.md` before acting.
- **Task-specific reads**: additionally read `.omp/instructions/ZEPHYR_PROJECT_STANDARD.md` for Zephyr build, DTS/Kconfig, driver, or board work; `.omp/instructions/ZEPHYR_DEBUG_PLAYBOOK.md` for target runtime/debug/flash work; `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md` for display/camera/audio/storage/network/NPU/DMA/performance paths; `.omp/instructions/REFACTORING_STANDARD.md` before planning or executing public API, architecture, module-boundary, data-model, hardware-sensitive, or multi-subsystem refactors; and `.omp/instructions/REFERENCES_POLICY.md` for reference material or copied vendor code.

## Collaboration Preferences

- Act as advisor and devil's advocate; point out blind spots directly.
- Auto-execute P0/P1 fixes, bug fixes, and small refactors when the correct action is clear.
- For a new driver or hardware module, first ask whether an existing upstream driver or small shim can replace self-developed code.
- Require confirmation for framework choices, architecture changes, data/API model changes, features outside roadmap, and large refactors.
- Never self-decide destructive actions such as deleting projects, production deploys, or financial operations.
- Read silently when context gathering is needed, then respond directly with findings, actions, or blockers.

## Task Classification

Classify the task before acting. Use the narrowest verification path that still proves the claim.

| Task Type                            | Examples                                                                                                                                   | Required Evidence Before Completion                                                                                                                                                                                                                                                   |
|--------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Research / Report                    | Codebase investigation, design critique, process report                                                                                    | Read relevant files; cite concrete files, commands, or observations. No build/flash required unless live hardware behavior is part of the answer.                                                                                                                                      |
| Hardware compatibility exploration   | Initial feasibility check for a display, sensor, radio, storage device, shield, connector, or SoC peripheral before implementation         | Treat as research by default. Read datasheets, schematics, reference BSPs, Zephyr bindings/drivers/Kconfig/DTS, and project memory. Report compatibility, risks, unknowns, and the smallest later validation step. Do not build, flash, debug, or write code unless explicitly requested. |
| Source-only firmware change          | Pure logic, parsers, small host-independent refactors                                                                                      | Build/test where feasible, code review for meaningful C/C++ changes. Flash only if runtime behavior changes.                                                                                                                                                                          |
| Refactor planning                    | Requested refactor where scope, public API impact, module boundary, behavior invariants, or verification path is unclear                  | Produce a plan with refactor class, invariants, callsite map, affected contracts, verification path, risks, rollback path, and confirmations needed. Do not edit, build, flash, debug, update memory, or create evidence unless explicitly asked to execute after the plan. |
| Refactor execution                   | Mechanical/internal refactor, or approved public API/module-boundary/hardware-sensitive refactor                                         | Check references/callsites, preserve behavior invariants, update affected tests/docs/callsites, run host tests/build/review as applicable, and use the hardware behavior gate for hardware-sensitive refactors.                              |
| Hardware behavior change             | Drivers, DTS/Kconfig affecting peripherals, IRQ/DMA/cache, board bring-up                                                                  | Build, review, flash, runtime verification on target, and evidence record.                                                                                                                                                                                                             |
| Host tooling / scripts               | build/flash/debug/monitor scripts                                                                                                          | Syntax check, dependency checks, dry-run or actual run. If it controls hardware, verify on target when available.                                                                                                                                                                      |
| Documentation / memory maintenance   | README, docs, codemap, memory updates, user-facing reports                                                                                 | May document already-verified facts. `docs/` content must be sanitized before writing or maintenance; omit personal data, local absolute paths, private tool paths, probe serials, and user instructions. Mark assumptions or blocked hardware work explicitly.                                      |
| Git hygiene                          | `.gitignore`, commit preparation, branch cleanup                                                                                           | Allowed after relevant verification exists or when explicitly requested. Never include build outputs, local logs, SDK paths, session exports, or unredacted documentation/source notes unless explicitly requested and safe.                                                            |

### Documentation Sanitization

- Before writing or updating `docs/`, separate factual engineering content from unredacted source material.
- `docs/` should focus on code structure, debug observations, usage steps, verification status, risks, and next technical actions.
- Do not write user prompts, process instructions, personal information, local absolute paths, private SDK/toolchain paths, probe serials, hostnames, credentials, or session artifacts into `docs/`.
- Treat unredacted notes and logs as non-maintained working material by default: do not stage or commit them, and do not copy them into `docs/`.
- If a report needs commands or paths, use repository-relative paths and neutral placeholders such as `<build-dir>`, `<venv>`, or `<toolchain>`.

### Performance / Streaming Optimization

For camera, audio, display, storage, network, NPU, or other streaming performance work, use `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md`. Evidence must include baseline/current metrics, measurement overhead, generated config, map/ELF placement, data correctness, buffer ownership, cache/memory state, and residual risks.

Status vocabulary for hardware work is canonical in `.omp/instructions/STATUS_AND_EVIDENCE.md`. Do not mark hardware-dependent work complete unless runtime verification passed. Use the exact status values Implemented, Built, Flashed, Runtime Verified, Visual Pending, Blocked, or Abandoned; describe missing evidence in prose.

## Memory System

Memory files live under `memory/` and track durable project state.

| Layer | File | Purpose |
| --- | --- | --- |
| Index | `memory/index.md` | Index for project memory files. |
| Current | `memory/current.md` | Active tasks, status, blockers, recent evidence links. |
| Project | `memory/project.md` | Durable project facts, hardware notes, architecture, build commands. |
| References | `memory/references.md` | Reference material index and summaries. |
| Pitfalls | `memory/pitfalls.md` | Root causes, quirks, timing/protocol constraints, board-specific lessons. |
| Evidence | `memory/evidence/` | Verification records for hardware-dependent work. |
| Daily | `memory/daily/` | Optional daily summaries and session rollups. |

`references/` stores read-only source material. `memory/references.md` indexes and summarizes that material. The canonical references policy is in `.omp/instructions/REFERENCES_POLICY.md`.

### Project Fact Discovery

When a hardware fact is discovered during bring-up, record it in the narrowest durable place instead of hardcoding it into this template:
- Board-wide facts, build commands, verified pin maps, runner details, and console addresses belong in `memory/project.md` or a board-specific memory file.
- Non-obvious mistakes, wrong assumptions, schematic mismatches, reset/power quirks, timing limits, and driver/framework workarounds belong in `memory/pitfalls.md` immediately.
- Generated-file facts that prove behavior, such as selected pins in `zephyr.dts`, Kconfig symbols, IRQ numbers, DMA/cache placement, or ELF symbols, belong in `memory/evidence/` for the task that verified them.

For new boards or external headers, produce a short board facts section in project memory after discovery. It should include power rails, console/log backend, flash runner, verified physical-header-to-SoC pin mapping, shared/alternate-function conflicts, and known-good build/flash/monitor commands.

## Verification Gate

For hardware behavior changes, Build -> Review -> Flash -> Runtime Verify must happen before completion docs, completion memory, git cleanup, or final completion claims.

Allowed before the gate:
- Code changes required to solve the task.
- Build/test/flash/debug commands.
- Reading logs, schematics, datasheets, generated files, memory, and code.
- Updating `memory/pitfalls.md` for newly discovered root causes or non-obvious quirks.
- Updating `memory/evidence/` with factual observations, without claiming completion.
- Updating `memory/evidence/*pending*.md` or `memory/current.md` with explicit partial states such as Built, Flashed, Runtime Verified, Visual Pending, or Blocked.

Required order:
1. Implement the smallest correct change.
2. Build and read output.
3. Review meaningful firmware/C/C++ changes.
4. Flash the verified build when hardware validation is required.
5. Runtime verify on the target or appropriate simulator when behavior depends on runtime hardware/software interaction.
6. If behavior is visible/physical, get visual or measured confirmation before marking complete.
7. Update docs/memory and do git maintenance only after the applicable gate passes.

For Zephyr, inspect `.config`, `zephyr.dts`, generated headers, map/ELF symbols, or section placement when behavior depends on Kconfig, DeviceTree, IRQs, DMA, cache, linker sections, or memory regions. For streaming or DMA paths, also use `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md`.

When Kconfig, Devicetree, linker sections, generated headers, board definitions, module manifests, or device dependencies change, use a fresh build directory or pristine build before claiming build evidence. Incremental build evidence is not sufficient for generated-file or dependency changes.

If embedded review finds a correctness issue after a build, fix it and rebuild before flashing. If runtime verification covers code changed after the first review, run a final focused review before completion.

For multi-candidate hardware experiments, embedded review must gate each candidate separately before flash. The review verdict for every candidate should be one of:
- **cleared**: no source-level blocker for the stated flash/runtime scope.
- **experimental only**: safe to flash for a narrow observation, but not valid as productization evidence without extra proof.
- **blocked**: do not flash until the named issue is fixed.

Do not let a cleared baseline imply that unrelated overlays, Kconfig variants, drivers, or UI samples are cleared.

Hardware compatibility exploration is exempt from the build/flash/runtime gate until it becomes implementation or validation work. Use `/hardware-compatibility` for initial module feasibility checks and stop at a report unless the user asks to implement or validate.

When recording evidence, use `.omp/instructions/STATUS_AND_EVIDENCE.md` and include the git commit if one exists and the evidence is tied to a committed revision.

For display, LED, audio, actuator, sensor, or other physical work, do not treat logs alone as complete unless logs are the intended output. Use Visual Pending until the user, a camera, a measurement instrument, or another appropriate observation confirms the physical behavior.

## Warning Triage

When build or runtime passes with warnings:
- Record the warning text or a precise summary.
- Classify each warning as blocking, accepted for this task, existing unrelated issue, or follow-up required.
- Do not hide warnings from the final report.

## Pitfalls Exception

`memory/pitfalls.md` is exempt from the completion gate. Update it immediately when a non-obvious root cause, framework quirk, timing constraint, hardware constraint, board routing issue, or failed assumption is discovered.

Pitfall entries should include:
- **Observation**: what was seen.
- **Root cause**: why it happened, if known.
- **Fix / workaround**: what changed or how to avoid it.
- **Lesson**: what to check earlier next time.

## Git Branch Safety

Before switching branches, creating branches, staging, committing, or other git hygiene:
- Run `git status --short --branch` and identify untracked/modified files.
- If the target branch does not exist, ask whether to create it, stay on the current branch, or use another branch name.
- Do not stash, reset, checkout, clean, or discard worktree changes unless explicitly requested.
- If switching with a dirty worktree is requested and safe, preserve the current modifications and state that they remain uncommitted.

## Session Continuation

When resuming from a session transcript:
- Extract the current goal, modified files, last verified evidence, remaining blockers, and commands already run.
- Do not blindly trust completion claims in the transcript.
- Re-run the smallest verification needed before marking completion.
- Do not commit session transcript files unless explicitly requested.
