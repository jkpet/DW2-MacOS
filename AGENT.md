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
  - `host/main.c` no longer uses Linux-only `MAP_32BIT` paths on macOS.
  - `tools/build_native.py` docs now mention macOS arm64 bring-up status and expected build directory.
- 2026-11-02: **Phase 2 initial implementation started**
  - `host/main.c` now adds a macOS-specific low-stack strategy using `mmap` low-address probing plus `pthread_attr_setstack`.
  - Both headless path and threaded/window path now run `game_main` on a thread whose stack is explicitly reserved below 2 GB (or fail fast with a clear error).
  - Low-stack probing was widened and hardened (broader address scan, smaller step, runtime fallback when `MAP_FIXED_NOREPLACE` is rejected, first-error errno logging).
  - Added an explicit second-pass `MAP_FIXED` fallback on macOS when hint-based `mmap` never lands below 2 GB, with dedicated fallback diagnostics.
- 2026-11-02: **Phase 4 partial progress**
  - `host/update.c` now makes macOS updater state explicit: self-updater disabled with a clear startup log, while update checking continues where libcurl is available.
  - macOS libcurl loading now probes `.dylib` names; Linux keeps `libcurl.so.4`.
- 2026-11-02: **Phase 1 verification hooks added**
  - `host/overlay.c` now validates overlay ranges at startup (bad range and overlap checks) before snapshotting data.
  - `CMakeLists.txt` now emits a macOS linker map (`build/.../dw2.map`) to inspect Mach-O overlay section layout.
- 2026-11-02: **macOS linker adjustment (revised)**
  - Full fixed-base Mach-O flags caused immediate process kill before `main` on arm64.
  - `-pagezero_size` also caused immediate loader termination on this arm64 setup; macOS link flags were reverted to map-only (known launch-safe baseline).
- 2026-11-02: **arm64 compatibility unblocked for iterative refactor**
  - `host/main.c` now allows an explicit experimental macOS arm64 path when low-stack reservation fails: continue with default pthread/SDL thread stacks and print clear warnings instead of hard exit.
  - `check_window` on macOS now warns (rather than aborting) when image/stack addresses are above 2 GB, to expose next pointer-truncation crash sites for targeted fixes.
- 2026-11-02: **macOS arm64 runtime investigation continued**
  - `check_window` errors name the failing probe. Repeated launches confirmed PIE ASLR could place the static 2 MB `Ps1_Ram` across a 16 MB boundary. Mach-O `-no_pie` is ignored for arm64, so this was fixed with a dedicated arena rather than linker flags (details below).
  - Fixed the first observed native pointer truncations in `Sys_Main`: pass the boot `RECT`, TIM data pointer, and VBlank callback as pointers on native builds.
  - Added arm64 sound pointer/function handle translation for the 32-bit values retained by translated libsnd/libspu code. Extended pointer-sized return/storage through the CD cache and native heap path, and fixed `Mem_GetOffsetEntry` to preserve the native pointer.
  - Replaced raw task pointers in fixed 32-bit slots with generation-tagged task references on 64-bit native builds. Updated task creation, dispatch, child traversal, lookup, destruction, host task inspection, and task-reference consumers; widened `Task_Create`'s separate init argument to `sptr` and converted its pointer-valued call sites.
  - Fixed the next startup crash in `ClearImage2`: two callers passed `RECT *` through an `s32` cast. Updated the Stg10 shadow init to retain its parent as a native pointer and the main shadow init to store a task reference.
  - Continued through title startup: fixed native pointer arithmetic in `Cd_GetFileEntry`/`Cd_GetFileEntrySubPtr`, changed the parts-drawing APIs to take pointers, fixed `LoadImage` call sites, kept OT-layer pointers native-width, and widened writes to the native packet cursor.
- 2026-11-02: **city-entry pointer audit continued**
  - Kept the VAB body pointer native-width at `SsVabTransBody`; this removed the intermittent startup segfault observed at that call.
  - Kept map-file pointer words as file-relative offsets on 64-bit builds and added typed accessors that resolve them against the map file base. This avoids writing truncated native pointers into the packed map header.
  - Widened the native `Actor_RefreshTransform` seam and added model-table accessors that resolve 32-bit animation/model offsets against their asset base rather than storing host pointers in file words.
  - Text interpretation now restores the host region for its legacy 32-bit text/return-stack pointers. Animation tables use file-relative accessors on 64-bit builds.
  - `CdLruEntry.data` is pointer-sized and compile-time checked against `CdCacheEntry`; `P32` now restores the high 32 bits from `Ps1_Ram` for raw host pointer words.

### Latest verified runtime state (current macOS arm64 machine)

- macOS arm64 release build succeeds (`./.venv/bin/python tools/build_native.py --arch arm64 --release`; latest full rebuild reported 1435 warnings across 127 files).
- Running `build/macos-arm64_rel/gte_test` currently exits with 1248 GTE command-reference mismatches; the test and GTE implementation were not changed in this investigation, and this result has not been compared with another platform.
- `alloc_low_stack()` still fails with `MAP_FIXED` errno 12, so runtime uses the experimental default-stack path.
- macOS arm64 now reserves a 16 MB-aligned, non-fixed `mmap` arena within the executable's 4 GB pointer region. `Ps1_Ram` occupies its first 2 MB and the three `Gpu_OtBufs` follow it; runtime checks ensure both ranges share the OT-link window and that legacy `P32` pointers still share a 4 GB region with the image. Twelve fresh one-VBlank launches passed with the arena at `0x110000000`.
- A scripted new-game smoke (`Start`, `Up`, `Cross`) reached game mode `0x307`, loaded city assets, and completed 600 VBlank waits. Verified after the scratchpad-region check: exit 0, 607 total waits, 7,669 Psy-Q calls, 302 flips. Full log: `/var/folders/5s/p6dvxjss1h1d672sc1njbb780000gp/T/opencode/dw2-city-smoke.log`.
- Real STR playback exposed a truncation at `Stg10_StrInit`: `(s32)Stg10_StrRingBuf` discarded the upper address bits before `StGetNext` wrote movie sectors. Native builds now pass the pointer intact; PS1 builds retain the original 32-bit cast. Headless `--movies` and windowed runs both pass 600 requested VBlank waits with exit 0. Windowed log: `/var/folders/5s/p6dvxjss1h1d672sc1njbb780000gp/T/opencode/dw2-window-movie-after.log`.
- The later post-dialogue crash was in `Gfx_ReleaseTexSlot`: LLDB received `0x22783c`, the low 32 bits of `Gfx_TexSlots[14]` at `0x10022783c` (`Gfx_TexSlots` begins at `0x10022767c`). Native release restores the executable's 4 GB pointer region before dereferencing legacy slot words; the stag0000 font's native pointer field is restored at assignment too. An LLDB probe set slot 14 to `-2`, called release with `0x22783c`, and confirmed the slot reset to 0.
- The fast, movie-enabled New Game script exposed another fault in `Digi_GetDefaultName`: it added an asset offset to a 64-bit file base in an `s32`. The helper now uses pointer-width address arithmetic. The scripted route entered the name and dialogue scenes, reached game modes `0x307`, `0x308`, and `0x301`, then completed 6,000 requested waits (6,054 VBlanks, 52,654 Psy-Q calls) with exit 0. Log: `/var/folders/5s/p6dvxjss1h1d672sc1njbb780000gp/T/opencode/dw2-scripted-dialogue-6000-after-namefix.log`.
- Dungeon entry exposed un-restored 32-bit file pointers in `Stg40_RelocDungFile`: the user's trace faulted at `0x100ad714` while the loaded file was at `0x110ad0d8`. The relocator now uses `P32` for floor/layout relocation records, and all `floorMap` reads use the same restoration. `tests/dungfile_reloc_test.c` reproduces the production relocation above 4 GB; it failed at `0x10000020` before the fix and passes afterward. The arm64 release build and a 600-wait headless movie startup smoke pass. The full user-driven dungeon route has not yet been rerun.
  - `[host] macOS low stack MAP_FIXED failed first with errno=12 (Cannot allocate memory)`
  - `[host] macOS: could not reserve a game stack below 2 GB`
  - `[host] macOS: continuing with default pthread stack (experimental arm64 compatibility mode)`
  - The initial task-slot crash (`Task_Run` received `0x33fd68` instead of `0x10033fd68`), VAB body pointer, map start-record pointer, actor transform argument, model table pointer, text pointer, animation table pointer, LRU cache data layout, and `P32` raw pointer failures are resolved along the tested startup/new-game path.
- The first `ClearImage` stack-pointer crash, sound DMA callback crash, CD-ready callback crash, and sound-buffer pointer crashes were each traced and passed by targeted pointer-width fixes. Raw 32-bit pointer words outside the task-reference system may still truncate.
- Fixed-base/pagezero experiments were reverted after loader termination; a standalone `-Wl,-no_pie` probe also confirmed Apple's linker ignores non-PIE for arm64.
- Disc image was restored by the user after an accidental shell `rm` command that included extra arguments.
- The PS1 objects-only regression build could not run because `mips-linux-gnu-as` is unavailable at `tools/macos/binutils/mips-linux-gnu-as` and no MIPS assembler is on `PATH`.

---

## Known blockers (current state)

1. **Mach-O overlay reset coverage**
   - Runtime section markers are valid and stag1000/stag2000 reloads passed. Exercise the remaining overlays and more scene transitions.
2. **Low-address stack strategy verification**
   - Low-address reservation still fails; the fallback is verified and exposes pointer truncation sites.
3. **Remaining native pointer-width verification**
   - Task slots use generation-tagged references, and the aligned arena passes the 600-VBlank new-game path. Continue exercising battle scenes and audit raw 32-bit pointer words exposed by further gameplay.
4. **macOS toolchain/SDK dependency**
   - Bring-up requires Xcode Command Line Tools (`clang`, `make`, SDK paths) to pass CMake compiler checks.
5. **Updater platform definitions**
   - `host/update.c` only assigns updater assets for Win64 and Linux x64; macOS path defaults to disabled updater.

---

## Implementation checklist (phased)

## Phase 0 — Baseline and feature gate

- [x] Add explicit platform detection for macOS in native build flow:
  - `CMakeLists.txt`: split `APPLE` from generic non-Windows branch.
  - `tools/build_native.py`: add Darwin target/build-dir naming.
- [ ] Introduce a compile-time feature define (e.g., `DW2_PLATFORM_MAC`) where needed, without changing Win/Linux logic.
- [x] Ensure all new behavior is additive and gated per platform.

**Exit criteria:** project config reaches generation stage on macOS with clear, deterministic errors for remaining blockers (not ambiguous linker/runtime failures).

## Phase 1 — Build/link compatibility on Mach-O

- [ ] Audit and replace GNU-ld/ELF-only options under `APPLE`:
  - overlay relink command (`ld -r -T script`) compatibility strategy.
  - final link options (`-Ttext-segment`, map options, PIE flags) replaced with mac-safe equivalents.
- [ ] Keep current overlay data reset semantics:
  - Preserve section bracketing behavior used by `host/overlay.c`.
  - If section naming differs on Mach-O, adapt marker/export strategy accordingly.
- [x] Add overlay verification hooks for bring-up diagnostics:
  - runtime range validation in `host/overlay.c`
  - linker map output on macOS (`dw2.map`)
- [ ] Ensure SDL3 package discovery works on macOS:
  - Prefer CMake config package from Homebrew or explicit path.

**Exit criteria:** `tools/build_native.py` can configure and compile linkable binaries on macOS arm64.

## Phase 2 — Runtime memory model adaptation (highest risk)

- [x] Rework Linux-only low-stack path in `host/main.c`:
  - Remove unconditional non-Windows `MAP_32BIT` dependency.
  - Implement macOS-specific low-address stack strategy **or** safe fallback path.
- [ ] Preserve address constraints validated by `check_window()`:
  - If macOS cannot satisfy existing below-2GB assumptions directly, introduce a controlled compatibility mode and document behavior impact.
- [x] Add explicit startup diagnostics:
  - Print which memory strategy is active.
  - Fail fast with clear message if invariants cannot be satisfied.

**Exit criteria:** macOS binary launches, passes early memory checks, and reaches game loop without undefined pointer truncation behavior.

## Phase 3 — Script/tooling and docs

- [x] Extend `tools/build_native.py` docs/help text for macOS usage.
- [x] Update `README.md` build section with macOS prerequisites and command.
- [ ] Keep existing Windows/Linux instructions untouched except where clarification is needed.

**Exit criteria:** new contributor can build macOS target from README alone.

## Phase 4 — Optional parity (updater/release)

- [ ] Add macOS updater asset mapping in `host/update.c` (if release artifacts are planned).
- [ ] Define naming convention for macOS release package.
- [x] Verify updater is disabled gracefully until release pipeline is ready.

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

### Mach-O overlay verification checklist (on macOS host)

After a successful macOS configure/build, inspect `build/macos-arm64*/dw2.map` and runtime logs:

1. **Map exists**
   - Confirm `dw2.map` was emitted in the active build directory.
2. **Overlay sections are present**
   - Check for the expected overlay section names:
     - `__dw2ovl0` .. `__dw2ovl6`
3. **Overlay ranges are non-empty**
   - Confirm each overlay section has non-zero size in the map output.
4. **Runtime range checks pass**
   - On startup, verify no `[host] overlay ... bad data range` or overlap errors.
5. **Snapshot/reset path is exercised**
   - Trigger at least one scene change and confirm no overlay-reset crash.
6. **Keep artifact references**
   - Save the map file path and relevant startup log lines in the session notes for handoff.

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
3. Rebuild incrementally and run under LLDB to capture the current failure:
   - `./.venv/bin/python tools/build_native.py --arch arm64 --release && lldb --batch -k 'thread backtrace all' -o 'run --no-window --vblanks 1 --disc "<restored cue path>"' -- ./build/macos-arm64_rel/dw2`
4. Task references and the stable 16 MB PS1/GPU arena are implemented; continue the headless run to the next concrete failure and validate battle/task consumers and remaining raw pointer words.
5. Keep commands chained with `&&` when giving copy/paste command sequences to avoid accidental path deletion.
6. Continue with the next unchecked phase item.
7. Preserve platform gates and avoid broad refactors unrelated to macOS bring-up.
