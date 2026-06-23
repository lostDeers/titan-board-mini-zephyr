---
description: Review embedded firmware, scripts, docs, evidence, and references-policy changes
---
# Embedded Review

Review the current changes for embedded firmware correctness: $ARGUMENTS

## Required Invocation

Use before committing or finalizing any meaningful change to:
- Embedded firmware source or headers: `*.c`, `*.h`, `*.cpp`, `*.hpp`.
- Zephyr board files, DTS/pinctrl, overlays, Kconfig, `prj.conf`, CMake, or module metadata.
- Build/flash/debug/monitor scripts and host tooling that affects target workflows.
- README/docs/memory/evidence files that state hardware status or verification.
- Reference policy, copied vendor/reference code, or active build inputs.

This review is not optional for embedded code changes. If the task tool is available, delegate the review to the `embedded-reviewer` agent and provide the current diff context. Otherwise, apply the same checklist directly.

## Review Scope

Select only the relevant review modes for the changed files. Findings first, ordered by severity. Include exact file references and residual verification gaps.

## Output

If no blocking findings exist, report `No CRITICAL or HIGH findings.` Then list:
- Residual risks.
- Verification gaps.
- Warnings accepted or requiring follow-up.
