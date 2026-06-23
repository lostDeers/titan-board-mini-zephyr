# Status And Evidence

Use this file as the canonical status vocabulary and evidence schema for OMP embedded workflows.

## Hardware Status Vocabulary

| Status | Meaning | Allowed completion claim |
| --- | --- | --- |
| Implemented | Code or configuration exists, but it has not necessarily been built. | Do not claim build, flash, or runtime behavior. |
| Built | The relevant build completed successfully for the stated app, board, and build directory. | Claim build success only. Do not claim hardware behavior. |
| Flashed | The built artifact was programmed and verified by the runner. | Claim flash success only. Do not claim runtime behavior. |
| Runtime Verified | Behavior was verified on the actual target or an appropriate simulator with exact logs, counters, register values, IDs, signals, or equivalent observations. | Hardware behavior may be claimed only for the verified scenario. |
| Visual Pending | Runtime logs or instrumentation passed, but the physical effect has not been observed by a human, camera, instrument, or equivalent measurement. | Claim runtime log success only; do not claim visible/physical completion. |
| Blocked | Work cannot continue with available tools or hardware. The blocker and missing evidence are recorded. | Claim the blocked state and completed prerequisite evidence only. |
| Abandoned | Work was deliberately dropped with a reason. | Claim abandonment only. |

Do not mark hardware-dependent work complete unless it is Runtime Verified. If behavior is visible or physical, keep Visual Pending until the physical effect is observed or measured.

## Evidence Record Schema

Use `memory/evidence/yyyy-mm-dd-short-task-name.md` for completed or durable hardware-dependent evidence. Prefer stable task names; avoid transient status words such as `built`, `flashed`, `runtime-verified`, `visual-pending`, or `blocked` unless the record is intentionally immutable for that exact state. If an earlier partial record used a transient status suffix and the status later changes, create a new final record with a stable name or state at the top that the filename is stale and the file content/status is authoritative. Evidence content must use only the canonical status vocabulary above. Use `memory/current.md` for active partial status when a full evidence record would imply more completion than was observed.

Required sections:

| Field | Content |
| --- | --- |
| Context | App, board, core, build dir, toolchain, probe, console backend, connected hardware revision, git commit/base, source state, and external workspace/module state when evidence is tied to uncommitted or externally patched sources. |
| Build | Full command, PASS/FAIL, artifact path, FLASH/RAM usage when available, and warnings. |
| Config | Relevant `.config` symbols and where they are set. |
| Devicetree | Relevant `zephyr.dts` nodes, IRQs, clocks, pinctrl, memory regions, chosen nodes, and device status. |
| ELF / Map | Relevant symbols, sections, buffer addresses, ISR symbols, DMA/cache-sensitive placement, and control metadata placement. |
| Flash | Command, runner, artifact, reset behavior, and Program/Verify result. |
| Runtime | Exact logs, register values, IDs, counters, bus ACK/NACK, visual behavior, or measured signals. |
| Warning Triage | Each warning classified as blocking, accepted for this task, existing unrelated issue, or follow-up required. |
| Residual Risks | Known limitations, hardware prerequisites, next debug step, and unverified branches. |

For multi-candidate hardware experiments, add a candidate matrix before detailed evidence:

| Candidate | Build dir/artifact | Config/DTS delta | Flash status | Runtime status | Visual/measured status | Decision |
| --- | --- | --- | --- | --- | --- | --- |

Also record:

- **Final flashed artifact**: exact build dir and image path currently on the target.
- **Adopted path**: the candidate selected for continued work, if any.
- **Rejected paths**: candidates rejected with metric, runtime, risk, or review evidence.

For physical or visible behavior, add human/camera/instrument observations:

| Order/time | Observer/source | Artifact | Observation | Action/status change |
| --- | --- | --- | --- | --- |

User visual reports are evidence. Record both failures and confirmations; do not overwrite earlier observations that caused a fix.

If evidence is tied to an uncommitted working tree or externally patched Zephyr/module workspace, record the dirty source state and any external module or Zephyr patches needed to reproduce the artifact. A commit hash alone is insufficient for reproducibility in that case.

## Performance / Streaming Evidence Addendum

For camera, audio, display, storage, network, NPU, motor control, sensor streaming, or other producer/consumer data paths, also record:

| Field | Content |
| --- | --- |
| Baseline | Previous build dir, command, metric, and whether validation/logging/host overhead was included. |
| Current Metric | Same metric after the change, measured on the same app path with comparable settings. |
| Measurement Overhead | Validation cadence, full-buffer scans, CRCs, RTT/UART logs, image saves, host rendering/saving, or other known measurement costs. |
| Data Correctness | CRC-valid frames, nonconstant DMA buffers, bounded dropped counters, monotonic samples, protocol status, or equivalent proof. |
| Buffer Ownership | Producer-owned buffers, consumer-owned buffers, ownership transfer points, release responsibilities, and overflow policy. |
| Cache / Memory | Cache state, memory-controller state, DMA/NPU/network/display driver symbols, buffer regions, alignment, and flush/invalidate boundaries. |

## User-Requested No-Modification Mode

If the user explicitly forbids modifications, do not update `memory/`, `references/`, evidence files, or generated reports. Return an evidence record candidate in the final response instead.
