# Zephyr Project Standard

This reusable standard applies to Zephyr RTOS work: applications, board definitions, DeviceTree, Kconfig, overlays, drivers, build/flash/debug scripts, and documentation.

## Mandatory Discovery

Before editing a Zephyr project, identify the active target context:
- Repository root.
- Zephyr workspace style: west workspace, standalone app, or module repository.
- Zephyr base path from `$ZEPHYR_BASE`, `west topdir`, or project docs.
- SDK/toolchain path from `$ZEPHYR_SDK_INSTALL_DIR`, CMake cache, or project docs.
- Active application directory.
- Active board name and qualifier.
- Build directory.
- Board root and module roots.
- Required overlays, snippets, shields, or extra CMake/Kconfig args.
- Flash/debug runner and hardware probe.
- Console/log backend.

Do not assume `app/`, `build/`, `west build -b <board> .`, or a particular SDK path until verified.

## Recommended Read Order

Read project instructions first, when present:
- `AGENTS.md`
- `.omp/instructions/INSTRUCTIONS.md`
- `.omp/instructions/ZEPHYR_PROJECT_STANDARD.md`
- `.omp/instructions/ZEPHYR_DEBUG_PLAYBOOK.md`
- `.omp/instructions/TASK_ROUTING_MATRIX.md`
- `.omp/instructions/STATUS_AND_EVIDENCE.md`
- `README.md`
- Relevant `docs/`
- `memory/index.md`, `memory/current.md`, `memory/project.md`, `memory/pitfalls.md`

For hardware bring-up and driver debugging, read pitfalls or equivalent debug notes before editing code.

## Build Command Pattern

Use the project's documented command when available. Otherwise derive one from discovered context.

```bash
west build -b <board> -d <build-dir> <app-dir> -- -DBOARD_ROOT=<board-root>
west build -b <board>/<qualifier> -d <build-dir> <app-dir> -- -DBOARD_ROOT=<board-root>
west build -b <board> -d <build-dir> <app-dir> -- -DDTC_OVERLAY_FILE=<overlay>
west build -b <board> -d <build-dir> <app-dir> -- -DEXTRA_CONF_FILE=<conf-fragment>
west build --sysbuild -b <board> -d <build-dir> <app-dir> -- <extra-cmake-args>
west build -b native_sim -d <build-dir> <app-dir>
west build -p always -b <board> -d <build-dir> <app-dir> -- <extra-cmake-args>
```

Use pristine builds when configuration, DTS, board definitions, generated headers, linker sections, device dependencies, or module manifests changed. Incremental build evidence is not sufficient for generated-file or dependency changes.

Build evidence should include:
- Full command.
- PASS/FAIL from output.
- Relevant warnings or errors.
- FLASH/RAM usage when available.
- Artifact path.
- Relevant `.config`, generated headers, `zephyr.dts`, map file, or ELF symbols when behavior depends on them.

For sysbuild/multi-image projects, record image-specific artifacts such as `build/<image>/zephyr/zephyr.elf`, merged HEX files, bootloader images, and final flashed artifact.

For hardware compatibility exploration, do not run builds by default. First report whether the hardware appears feasible from bindings, drivers, Kconfig, DTS, SoC HAL support, schematics, datasheets, reference BSPs, and project memory. Recommend the smallest later build only if implementation or validation is requested.

## Flash And Debug Pattern

Use the project-documented runner when available. Otherwise inspect `west flash --context`, board docs, or existing scripts.

```bash
west flash -d <build-dir> --runner <runner>
west debug -d <build-dir> --runner <runner>
west debugserver -d <build-dir> --runner <runner>
```

Flash evidence must include command, runner, firmware artifact, reset behavior, and success/failure output.

Never claim hardware behavior is fixed only because flashing succeeded.

## Console And Runtime Verification

Identify the active console backend before relying on logs:
- UART console.
- SEGGER RTT.
- USB CDC ACM.
- Semihosting.
- Native simulator stdout.
- File logs or another documented backend.

Runtime evidence should include exact observations, such as boot banners, device IDs, bus ACK/NACK, IRQ/DMA counters, register values, timestamps, protocol status, visual results, or measured signals.

If warnings or errors appear while behavior still passes, classify them as blocking, accepted for this task, existing unrelated issue, or follow-up required.

## Kconfig Rules

- Keep configuration at the narrowest correct scope.
- Board fixed capabilities belong in board defconfig or board Kconfig defaults.
- Application requirements belong in `prj.conf` or app-specific fragments.
- Feature variants belong in clearly named fragments, snippets, or overlays.
- Prefer documented Zephyr Kconfig symbols over local defines.
- After changing Kconfig, rebuild and inspect generated `.config` when behavior depends on it.

## DeviceTree Rules

DeviceTree describes hardware, not runtime policy.

Use DTS for fixed buses, pins, interrupts, clocks, GPIOs, regulators, aliases, chosen nodes, hardware-present devices, static pinctrl, DMA, and memory regions.

Use overlays for variants, shields, debug-only wiring, and temporary experiments.

After changing DTS, inspect generated `zephyr.dts` when pin, bus, interrupt, memory, or device readiness behavior is uncertain.

## Driver Rules

Prefer existing Zephyr drivers when they work for the SoC, board wiring, and required behavior.

Before writing a custom driver, confirm:
- Existing Zephyr support and HAL compatibility.
- Required bindings and Kconfig symbols.
- Timing, reset, power, bus topology, and interrupt constraints.
- Whether a small board-specific shim is enough.

Do not directly compile or include driver code from `references/`. If reference implementation code is needed, copy the minimal required files or code blocks into active source, preserve license/header information, document origin, and adapt behind a project-local interface.

## Hardware Bring-Up Workflow

- Confirm schematic, pin map, rails, reset, enable/OE, boot straps, jumpers, and shared buses.
- Confirm the selected build targets the connected board and revision.
- Verify the simplest observable signal first: ID read, GPIO toggle, bus ACK, clock output, interrupt count.
- Add one diagnostic at a time and remove or gate temporary diagnostics after use.
- Record non-obvious root causes and board constraints in pitfalls/debug notes.

### Hardware Compatibility Exploration

Use `/hardware-compatibility` for initial feasibility checks before bring-up. This mode is research-first:
- Do not edit source, DTS, Kconfig, scripts, or docs unless explicitly requested.
- Do not build, flash, run monitors, attach debuggers, or perform target-side validation unless explicitly requested after the report.
- Check whether existing upstream Zephyr drivers, bindings, Kconfig symbols, and SoC HAL support can cover the hardware.
- Compare hardware facts from schematic, connector pinout, datasheet, board DTS/pinctrl, reference BSP, and project memory.
- Return a verdict, evidence, integration sketch, risks/unknowns, and the smallest recommended validation step.
- Do not create runtime evidence or mark hardware work complete from compatibility research alone.

### Interface And Header Mapping

For external connectors, shields, mikroBUS/PMOD/Grove/Raspberry-style headers, Arduino headers, camera/display headers, or board-to-board connectors:
- Treat physical pin numbers, connector labels, schematic net names, SoC pads, package pins, and peripheral functions as different facts until cross-checked.
- Confirm mappings from at least two available sources when possible: schematic, board pin config, generated `zephyr.dts`, FSP/CubeMX/pinctrl output, reference BSP, silkscreen, or measured continuity/signal behavior.
- Do not infer a physical connector pin from a symbolic name alone.
- Record the verified physical-header-to-SoC mapping and any shared alternate-function conflicts in project memory.
- When changing pinctrl for an external bus, inspect generated `zephyr.dts` and verify the electrical idle state or simplest bus transaction before attaching complex drivers.

### Display And UI Bring-Up

For OLED, TFT, e-paper, LED matrices, backlights, touch panels, or other visible outputs:
- Verify power voltage and signal logic level first.
- For I2C displays, check idle-high, run bus recovery when appropriate, probe addresses, then initialize the panel.
- For SPI displays, verify SCLK/MOSI/CS plus control GPIOs such as DC, RESET, BUSY, TE, or backlight before running a full graphics stack.
- Start with a minimal visible pattern or status screen before enabling complex UI rendering.
- Record visual confirmation separately from bus/log success. Use Visual Pending until the physical display is observed.

### Streaming And Real-Time Paths

For audio, camera, display refresh, network, storage, motor control, sensor streaming, or any producer/consumer firmware path, use `.omp/instructions/STREAMING_DMA_CACHE_CHECKLIST.md`. At minimum:

- Identify producers, consumers, buffer ownership, ownership transfer points, release responsibilities, and backpressure behavior before coding.
- Define ring-buffer and queue overflow policy explicitly: block producer, drop newest, drop oldest, overwrite, or fail-fast.
- Prefer continuous capture/transfer with ring or double buffers over repeated stop/start loops when samples must be contiguous.
- Do not pass pointers to mutable producer/DMA buffers to slow consumers unless an ownership/release protocol prevents mutation until release.
- Keep ISR work bounded and defer processing to threads/work queues.
- Throttle slow outputs such as displays and logs so they cannot starve capture or control loops.
- Rate-limit repeated error logs, and expose counters for overruns, dropped frames, read failures, and retries.
- Runtime evidence must include repeated successful cycles, data correctness proof, and absence or bounded handling of overrun/read-failed/update-failed logs.

## Zephyr Evidence Template

Use `.omp/instructions/STATUS_AND_EVIDENCE.md` as the canonical evidence schema and `.omp/templates/evidence.md` as the reusable record template. Evidence should summarize generated files; do not commit build directories or generated outputs unless explicitly requested.

## References Policy

Use `.omp/instructions/REFERENCES_POLICY.md` as the canonical references policy. Project `references/` directories are documentation/source archives, not active build inputs.

## Git And Generated Files

Before committing, ensure only intended source/config/docs/script/evidence-summary changes are staged.

Do not commit:
- `build*/`
- `*.elf`, `*.hex`, `*.bin`, `*.uf2`, `*.map`
- RTT/GDB logs
- local session exports
- SDK paths
- private reference drops

Only commit when explicitly requested.
