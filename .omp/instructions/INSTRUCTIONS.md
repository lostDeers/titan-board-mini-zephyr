# General OMP Instructions

## Required Reads

Before firmware, hardware, board, driver, or Zephyr documentation work, read:

- `AGENTS.md`
- `.omp/instructions/TASK_ROUTING_MATRIX.md`
- `.omp/instructions/STATUS_AND_EVIDENCE.md`
- `.omp/instructions/ZEPHYR_PROJECT_STANDARD.md`
- `.omp/instructions/ZEPHYR_DEBUG_PLAYBOOK.md`

Also read `.omp/instructions/REFERENCES_POLICY.md` before touching reference material, copied vendor code, provenance docs, CMake/source includes, or `memory/references.md`. Read `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md` for streaming, DMA, cache, NPU, camera, audio, display, storage, network, or motor/sensor producer-consumer paths.
Read `.omp/instructions/REFACTORING_STANDARD.md` before planning or executing public API, architecture, module-boundary, data-model, hardware-sensitive, or multi-subsystem refactors.

## Mandatory Routing And Gates

Before editing, classify the task using `AGENTS.md` and `.omp/instructions/TASK_ROUTING_MATRIX.md`, then select the narrowest verification gate that proves the change. Treat mixed tasks as the strictest applicable class, but split host-testable logic from hardware-only behavior.

Required routing:
- Meaningful changes to embedded C/C++/headers, Zephyr DTS/Kconfig/CMake/overlays, build/flash/debug/monitor scripts, memory/evidence, or reference policy MUST run `/embedded-review` or delegate to `embedded-reviewer` before commit/final response.
- Pure software logic, protocol encoders/decoders, parsers, register-layout helpers, shared-memory layouts, and reusable host tools MUST have host/unit tests when feasible. Prefer tests before implementation. If no project test runner exists, create the smallest project-local host test runner instead of skipping tests.
- Hardware behavior changes MUST follow Build -> Review -> Flash -> Runtime Verify before any completion claim. If hardware access is unavailable, report the exact status from `.omp/instructions/STATUS_AND_EVIDENCE.md`; do not imply Runtime Verified.
- Target-side runtime diagnosis, flash/runtime failures, probe/RTT/UART issues, IRQ/DMA/cache behavior, sensor/display/audio physical behavior, or any task that needs Runtime Verified status MUST use `/hardware-debug` or delegate to `hardware-debugger`.
- Zephyr build/CMake/Kconfig/Devicetree/linker failures SHOULD be routed to `/zephyr-build-fix` or delegated to `zephyr-build-resolver`; keep fixes minimal.
- Any cited build/flash/runtime observation SHOULD be converted into `memory/evidence/` or `memory/current.md` via `/record-evidence` or `evidence-recorder`. If the user explicitly forbids modifications, return an evidence record candidate instead.
- Any change touching `references/`, copied vendor/reference code, active build paths, CMake/source includes, or `memory/references.md` MUST run `/reference-check` or delegate to `reference-curator` before commit/final response.

Commit/final gate checklist is canonical in `.omp/instructions/TASK_ROUTING_MATRIX.md`.

## Coding Style

### File Organization

- Prefer many focused files over a few large files.
- Keep high cohesion and low coupling.
- Organize by feature/domain, not only by type.
- Keep application policy separate from board support and reusable drivers.

### Error Handling

- Handle errors comprehensively when debugging.
- Propagate init failures instead of continuing downstream initialization after upstream failure.
- For scripts, report failed commands with a useful log path or exact failing step.

## Testing Requirements

Use the strongest useful verification for the task. Do not create artificial tests only to satisfy a generic checklist.

For pure software logic, parsers, protocol encoders, algorithms, shared-memory/register layouts, and reusable host tools:
- Prefer TDD when practical.
- Add unit tests or integration tests where they provide real regression value.
- Aim for meaningful coverage instead of fake coverage.
- Run the narrowest host test runner that proves the changed logic.

For embedded firmware and hardware bring-up:
- Do not force unit tests when behavior depends on physical hardware, generated DeviceTree/Kconfig, timing, or board wiring.
- Still test host-testable pure logic extracted into headers/helpers.
- Define and execute the strongest feasible verification path: build, generated config/DTS inspection, flash, runtime logs/registers/signals, and warning triage.
- Record exact evidence before claiming runtime behavior is complete.

## Documentation Sanitization Workflow

Before writing or updating `docs/`:
- Keep the document focused on code, debugging, usage, verification status, risks, and technical next steps.
- Do not include user prompts, workflow instructions, sanitization explanations, personal information, hostnames, probe serials, credentials, local absolute paths, private SDK/toolchain paths, or session artifacts.
- Treat unredacted source notes and logs as non-maintained working material by default: do not write them to `docs/` and do not include them in git maintenance.
- Only sanitized content may be written to `docs/` and maintained over time.
- Use repository-relative paths and neutral placeholders such as `<build-dir>`, `<venv>`, or `<toolchain>` when commands need environment-specific values.

## Git Workflow

### Init

- If no git repo exists, use `git init` to create one.
- If a git repo already exists, do nothing.

### Commit Message Format

```text
<type>: <description>

<optional body>
```

Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `ci`.

Before committing embedded work:
- Check `git status --short`.
- Check staged and unstaged diffs.
- Run the mandatory routing/gate checklist from `.omp/instructions/TASK_ROUTING_MATRIX.md`.
- Do not commit build outputs, generated binaries, local logs, local SDK paths, credentials, or session exports unless explicitly requested.
- Do not commit unredacted reports, notes, logs, screenshots, terminal captures, or documents containing personal data, local absolute paths, private SDK/toolchain paths, probe serials, hostnames, credentials, or session artifacts.
- Summarize generated evidence in `memory/evidence/` instead of committing generated files.

Only commit when explicitly requested.

## Feature Implementation Workflow

1. Plan enough to identify dependencies, risks, verification path, and required command/agent routing.
2. For pure software logic, use tests-first when practical.
3. For embedded/Zephyr hardware work, define build/config/DTS/flash/runtime evidence instead of forcing artificial tests.
4. Implement the smallest correct change.
5. Run host/unit tests for host-testable logic.
6. Build and inspect generated config/DTS/ELF/map when behavior depends on them. Use a fresh/pristine build when Kconfig, DTS, linker sections, generated headers, or device dependencies changed.
7. Run embedded review for meaningful embedded changes.
8. Flash/runtime verify when hardware behavior is in scope; otherwise mark the exact partial status from `.omp/instructions/STATUS_AND_EVIDENCE.md`.
9. Record evidence/current memory after verification, unless the user explicitly forbids modifications.

## Refactoring Workflow

A refactor changes structure without intentionally changing externally observable behavior. If behavior changes, classify and verify the task by the behavior change, not as a refactor.

1. Classify the refactor using `.omp/instructions/REFACTORING_STANDARD.md`.
2. Use `/refactor-plan` before public API, architecture, data model, framework, module-boundary, hardware-sensitive, or large multi-subsystem refactors unless the user has already approved the plan.
3. Small mechanical/internal refactors may proceed when behavior invariants and verification are clear.
4. Identify exported symbols, callsites, ABI/layout/protocol/config contracts, init order, ownership, timing, and generated Zephyr state before editing.
5. Default to a clean cutover. Do not leave stale aliases, wrappers, duplicate APIs, dead comments, or unrelated formatting churn unless explicitly required.
6. Run `/host-test` for host-testable invariants when feasible.
7. Run fresh/pristine builds and inspect generated files when Kconfig, DTS, linker, generated headers, board definitions, module manifests, or device dependencies change.
8. Keep the hardware verification gate for hardware-sensitive paths; build success does not prove hardware behavior.
9. Run `/embedded-review` for meaningful embedded, Zephyr, script, docs/evidence, or reference-policy changes.

## Success Metrics

You are successful when:
- Relevant tests or hardware verification pass with evidence.
- No security vulnerabilities are introduced.
- Code is readable and maintainable.
- Performance and resource usage are acceptable for the target.
- User requirements and hardware constraints are met.
