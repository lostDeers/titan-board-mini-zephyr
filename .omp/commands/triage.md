---
description: Classify a task and produce the narrowest verification plan
---
# Triage

Classify and plan this task before editing: $ARGUMENTS

## Required Reads

Read `AGENTS.md`, `.omp/instructions/INSTRUCTIONS.md`, `.omp/instructions/TASK_ROUTING_MATRIX.md`, `.omp/instructions/STATUS_AND_EVIDENCE.md`, and any task-specific project memory or docs needed to avoid guessing.
Also read `.omp/instructions/REFACTORING_STANDARD.md` when the request includes or may require a refactor.

## Output

Return:

- Task classification.
- Required reads completed.
- Affected artifact classes.
- Required commands or agents.
- Verification gate.
- Evidence destination, or evidence candidate if the user forbids modifications.
- Destructive actions requiring confirmation.
- Smallest correct next action.

Do not edit files, build, flash, debug, or update memory from this command unless the user explicitly asks for execution after the triage result.
