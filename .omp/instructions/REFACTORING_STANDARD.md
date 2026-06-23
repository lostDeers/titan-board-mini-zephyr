# Refactoring Standard

Use this standard for behavior-preserving structural changes. A refactor changes structure without intentionally changing externally observable behavior. If behavior changes, classify and verify the task by the changed behavior, not as a refactor.

## Refactor Classes

| Class | Examples | Default route |
| --- | --- | --- |
| Mechanical | Private rename, file move, include cleanup, proven dead-code deletion. | `/refactor`; references/callsites checked; narrow build/test when feasible. |
| Internal source refactor | Function extraction, module split, local API cleanup, pure helper reorganization. | `/refactor`, `/host-test` when feasible, `/embedded-review` for meaningful embedded code. |
| Public API refactor | Exported headers, public symbols, script arguments, config schema, packet/layout/shared-memory/register contracts. | `/refactor-plan` first; execute only after approval; verify all callsites and contracts. |
| Embedded integration refactor | Init order, device ownership, thread/work queue/ISR callback boundaries, CMake/Kconfig/DTS organization. | `/refactor-plan` when scope is non-trivial; stricter Zephyr or hardware route if behavior can be affected. |
| Hardware-sensitive refactor | IRQ, DMA/cache, timing, bus transactions, buffer ownership, streaming, display/audio/sensor/storage/network/NPU paths. | `/refactor-plan`; then Build -> `/embedded-review` -> Flash -> Runtime Verify -> `/record-evidence` with canonical status vocabulary. |

## Confirmation Gate

Proceed without confirmation only for mechanical or small internal refactors where the behavior invariant and verification path are clear.

Require user confirmation before editing when the refactor affects:

- Public/exported APIs, headers, script interfaces, data models, packet formats, shared-memory layouts, register-layout helpers, or configuration schemas.
- Architecture, framework choice, module boundaries, ownership model, concurrency model, or hardware abstraction boundaries.
- Multiple subsystems where the clean cutover cannot be reviewed as one local change.
- Hardware-sensitive init order, ISR/thread context, DMA/cache, timing, buffer ownership, or streaming paths.

## Before Editing

Identify and record in the plan or working notes:

- Behavior invariants that must remain unchanged.
- Public/exported symbols and every callsite. Use language-aware references when available.
- ABI, layout, protocol, register, shared-memory, on-flash, or wire-format contracts.
- Init order, thread context, ISR context, ownership transfer points, queue/ring semantics, and timeout behavior.
- Zephyr generated-state impact: Kconfig, DeviceTree, CMake, linker sections, generated headers, board roots, modules, overlays, snippets, and sysbuild images.
- Hardware-sensitive paths: IRQ, DMA/cache, memory placement, bus transactions, display/audio/sensor/storage/network/NPU, timing, and physical outputs.
- Baseline verification that proves current behavior when feasible.

## Implementation Rules

- Default to a clean cutover. Avoid old/new APIs coexisting unless the user explicitly requires a compatibility window.
- Remove stale wrappers, aliases, duplicated paths, dead comments, and unused code after references and verification support removal.
- Do not mix unrelated formatting churn with a semantic refactor.
- Do not hide feature changes, bug fixes, retries, validation changes, timing changes, or warning suppression under a refactor label.
- Keep allocation, copying, stack use, ISR work, hot-path branches, and memory placement no worse unless explicitly accepted.
- Preserve license/provenance notes when moving copied reference-derived code.
- Run `/reference-check` for refactors touching `references/`, copied vendor/reference code, provenance docs, active build paths, CMake/source includes, or `memory/references.md`.
- Keep DTS describing hardware, not runtime policy; keep Kconfig at the narrowest correct scope.

## Verification Matrix

| Refactor impact | Required verification |
| --- | --- |
| Mechanical/private source only | References/callsites checked; narrow compile/test where available. |
| Host-testable logic | Add or run meaningful host/unit tests for invariants, edge values, layouts, and error paths. |
| Embedded C/C++/headers | Build or host-test as applicable, then `/embedded-review`. |
| Public API or layout contract | All callsites updated, tests for contract behavior/layout when feasible, docs updated when the public contract changes, `/embedded-review`. |
| CMake/Kconfig/DTS/linker/generated headers/module metadata | Fresh/pristine build when generated state or dependencies change; inspect `.config`, `zephyr.dts`, generated headers, map, or ELF symbols as relevant; `/embedded-review`. |
| Hardware-sensitive path or hardware behavior can be affected | Build -> `/embedded-review` -> Flash -> Runtime Verify; record evidence with canonical status vocabulary. |
| Streaming/performance path | Use `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md`; compare baseline/current metrics on the same app path and record data correctness, ownership, cache/memory, and residual risks. |
| Reference material or copied reference-derived code | Run `/reference-check`; preserve license/header information, origin path, adaptation notes, and project-local API boundaries. |

## Review Expectations

A refactor is reviewable when:

- The diff shows one structural intent.
- Behavior invariants are explicit.
- All callsites are cut over.
- Verification evidence matches the affected behavior.
- Remaining runtime or hardware gaps are classified with `.omp/instructions/STATUS_AND_EVIDENCE.md` instead of being described as complete.
