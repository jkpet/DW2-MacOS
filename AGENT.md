# AGENT: Apple Silicon (macOS arm64) bring-up plan for DW2-PC

This file captures the implementation approach to get this repository running on macOS Apple Silicon (arm64), with enough detail for handoff between agents.

## Goal

Produce a native macOS arm64 build of the DW2 native client (`dw2`) that:

1. Configures/builds from this repo without ad-hoc local patches.
2. Launches and reaches title/city scenes.
3. Preserves current runtime invariants (memory/window constraints) or introduces a safe equivalent.
4. Keeps Windows/Linux behavior unchanged.

## Current progress

- 2026-11-02: **Phase 0 started**
  - `tools/build_native.py` now has explicit Darwin detection, `arm64` arch selection, and macOS toolchain probing.
  - `CMakeLists.txt` no longer falls through Linux-oriented logic on macOS arm64.
- 2026-11-02: **Phase 1 scaffolding started**
  - `CMakeLists.txt` now separates Linux and macOS link behavior (`elseif(APPLE)` branches).
  - macOS overlay relink now has an initial Mach-O implementation attempt (`ld -r` + `-rename_section` for `__data`/`__bss` per overlay).
  - `host/overlay.c` now has a macOS marker path based on `section$start$...` / `section$end$...` symbols for the relinked overlay sections.
  - `host/main.c` now fails fast on macOS at runtime thread start with an explicit Phase 2 message instead of relying on Linux-only `MAP_32BIT` code paths.
  - `tools/build_native.py` docs now mention macOS arm64 bring-up status and expected build directory.

---

## Known blockers (current state)

1. **Linux/ELF assumptions in native build path**
   - Current non-Windows path in `CMakeLists.txt` is Linux-oriented (GNU-ld flags and ELF semantics).
2. **Low-address stack mapping dependency**
   - `host/main.c` uses `MAP_32BIT` for non-Windows to keep stack below 2GB.
   - `MAP_32BIT` is Linux-specific and unavailable on macOS.
3. **Build script platform coverage**
   - `tools/build_native.py` only treats Windows and Linux as first-class native build targets.
4. **Updater platform definitions**
   - `host/update.c` only assigns updater assets for Win64 and Linux x64; macOS path defaults to disabled updater.

---

## Implementation checklist (phased)

## Phase 0 — Baseline and feature gate

- [ ] Add explicit platform detection for macOS in native build flow:
  - `CMakeLists.txt`: split `APPLE` from generic non-Windows branch.
  - `tools/build_native.py`: add Darwin target/build-dir naming.
- [ ] Introduce a compile-time feature define (e.g., `DW2_PLATFORM_MAC`) where needed, without changing Win/Linux logic.
- [ ] Ensure all new behavior is additive and gated per platform.

**Exit criteria:** project config reaches generation stage on macOS with clear, deterministic errors for remaining blockers (not ambiguous linker/runtime failures).

## Phase 1 — Build/link compatibility on Mach-O

- [ ] Audit and replace GNU-ld/ELF-only options under `APPLE`:
  - overlay relink command (`ld -r -T script`) compatibility strategy.
  - final link options (`-Ttext-segment`, map options, PIE flags) replaced with mac-safe equivalents.
- [ ] Keep current overlay data reset semantics:
  - Preserve section bracketing behavior used by `host/overlay.c`.
  - If section naming differs on Mach-O, adapt marker/export strategy accordingly.
- [ ] Ensure SDL3 package discovery works on macOS:
  - Prefer CMake config package from Homebrew or explicit path.

**Exit criteria:** `tools/build_native.py` can configure and compile linkable binaries on macOS arm64.

## Phase 2 — Runtime memory model adaptation (highest risk)

- [ ] Rework Linux-only low-stack path in `host/main.c`:
  - Remove unconditional non-Windows `MAP_32BIT` dependency.
  - Implement macOS-specific low-address stack strategy **or** safe fallback path.
- [ ] Preserve address constraints validated by `check_window()`:
  - If macOS cannot satisfy existing below-2GB assumptions directly, introduce a controlled compatibility mode and document behavior impact.
- [ ] Add explicit startup diagnostics:
  - Print which memory strategy is active.
  - Fail fast with clear message if invariants cannot be satisfied.

**Exit criteria:** macOS binary launches, passes early memory checks, and reaches game loop without undefined pointer truncation behavior.

## Phase 3 — Script/tooling and docs

- [ ] Extend `tools/build_native.py` docs/help text for macOS usage.
- [ ] Update `README.md` build section with macOS prerequisites and command.
- [ ] Keep existing Windows/Linux instructions untouched except where clarification is needed.

**Exit criteria:** new contributor can build macOS target from README alone.

## Phase 4 — Optional parity (updater/release)

- [ ] Add macOS updater asset mapping in `host/update.c` (if release artifacts are planned).
- [ ] Define naming convention for macOS release package.
- [ ] Verify updater is disabled gracefully until release pipeline is ready.

**Exit criteria:** updater behavior is explicit and non-broken on macOS (enabled with valid asset, or clearly disabled).

---

## Execution order (recommended)

1. Phase 0
2. Phase 1
3. Phase 2 (risk spike; resolve before polishing)
4. Phase 3
5. Phase 4 (optional)

---

## Validation plan

For each phase, run the smallest relevant checks first:

1. Configure/build:
   - native build script invocation on macOS.
2. Smoke runtime:
   - launch with `--version`
   - headless short run (`--no-window --vblanks N`) once build runs.
3. Functional smoke:
   - launch windowed and reach title scene.
4. Regression confidence:
   - ensure Windows/Linux code paths remain untouched unless explicitly edited.

Keep logs for each checkpoint under `build/<platform>/` artifacts.

---

## Risk log

1. **Critical risk:** below-2GB pointer/stack assumptions may not be trivially enforceable on macOS arm64.
2. **High risk:** overlay relink/section semantics differ between ELF and Mach-O.
3. **Medium risk:** SDL3 package discovery variance across developer setups.

Mitigation strategy: isolate each risk behind compile-time gates, add explicit diagnostics, and avoid silent fallbacks.

---

## Handoff notes for future agents

When resuming:

1. Read this file first.
2. Check `CMakeLists.txt`, `tools/build_native.py`, `host/main.c`, `host/update.c` for partially completed phase work.
3. Continue with the next unchecked phase item.
4. Preserve platform gates and avoid broad refactors unrelated to macOS bring-up.
