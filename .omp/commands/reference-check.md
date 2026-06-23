---
description: Check references/ usage and update memory/references.md
---
# Reference Check

Check reference material usage for: $ARGUMENTS

## Required Invocation

Use before committing/finalizing when changes touch:
- Files under `references/`.
- Copied vendor/reference code in active source.
- CMake, scripts, includes, or build metadata that might reference `references/`.
- `memory/references.md` or documentation of reference provenance.

`references/` is read-only source material and must not be compiled, linked, included, or sourced directly by active firmware/scripts.
Use `.omp/instructions/REFERENCES_POLICY.md` as the canonical policy. `.omp/scripts/check_references_policy.py` may be used as a conservative local scan before the curator review.

## Workflow

If the task tool is available, delegate to the `reference-curator` agent. Verify that `references/` is not an active build input. If reference code is copied into active source, verify license/header preservation, origin documentation, and project-local API isolation.

## Output

Report:
- Active references found.
- Violations or confirmation of no active build dependency.
- Copied-code provenance/license status.
- Updates made or recommended for `memory/references.md`.
