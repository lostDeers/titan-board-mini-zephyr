# References Policy

`references/` stores read-only source material: datasheets, schematics, BSP archives, upstream examples, vendor notes, and processed reference notes. `memory/references.md` is the index and summary for that material.

## Rules

- Do not include, compile, link, or source files directly from `references/` in active firmware, scripts, CMake, Kconfig, Devicetree, or host tooling.
- If reference code is required, copy the minimal required files or code blocks into an active project directory.
- Preserve original license headers and copyright notices.
- Document the origin path, upstream source, adaptation notes, and why upstream Zephyr support or a small shim was not sufficient.
- Keep copied code behind a project-local API so it can be replaced later.
- Keep `memory/references.md` as an index, not an active dependency manifest.
- Before committing, inspect diffs for active build paths that point into `references/`.
- Do not commit private reference drops, proprietary blobs, credentials, or SDK paths unless explicitly requested and legally cleared.

## Review Requirements

Run `/reference-check` when changes touch:

- Files under `references/`.
- Copied vendor/reference code in active source.
- CMake, scripts, includes, Kconfig, Devicetree, or build metadata that might reference `references/`.
- `memory/references.md` or documentation of reference provenance.

## Automation

`.omp/scripts/check_references_policy.py` performs a conservative text scan for active dependencies on `references/`. It does not replace `/reference-check`; it catches low-level policy violations before review.
