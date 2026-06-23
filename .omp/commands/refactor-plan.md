---
description: Plan a behavior-preserving refactor before editing
---
# Refactor Plan

Plan a refactor for: $ARGUMENTS

This is a planning task by default. Do not edit files, build, flash, debug, update memory, or create evidence records unless the user explicitly asks for execution after the plan.

## Required Reads

Read `AGENTS.md`, `.omp/instructions/INSTRUCTIONS.md`, `.omp/instructions/TASK_ROUTING_MATRIX.md`, `.omp/instructions/REFACTORING_STANDARD.md`, and relevant source, tests, project memory, docs, or references needed to avoid guessing.

## Required Use

Use before public API, architecture, data model, framework, module-boundary, multi-subsystem, or hardware-sensitive refactors when the scope or verification path is not already clear.

## Output

Return:

- Refactor class from `.omp/instructions/REFACTORING_STANDARD.md`.
- Current structure and concrete pain points.
- Behavior invariants that must not change.
- Public API, ABI, layout, protocol, config, script, or hardware contracts affected.
- Callsite and dependency map, including exported symbols when relevant.
- Proposed clean cutover and files expected to change.
- Tests, builds, generated-file inspections, flash/runtime checks, reviews, and evidence records required.
- Risks, rollback path, and compatibility policy.
- User decisions or confirmations required before editing.
- Smallest safe first edit if approved.
