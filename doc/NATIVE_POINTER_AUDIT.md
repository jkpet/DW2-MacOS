# Native Pointer-Width Audit

Audit results for the macOS arm64 native build. Dungeon relocation, room-label flood-fill,
random-hazard cleanup, and floor packet cursor crashes were reproduced and fixed with focused arm64
regression tests. This audit broadens the scan to pointer-sized integer narrowing and records
remaining candidates and planned fixes.

## Audit method

The initial scan used Clang pointer-cast diagnostics over 163 C translation units under `src/`. That
found many explicit pointer/integer casts, but missed implicit narrowing when `sptr` values were
assigned to `s32`—the flood-fill queue bug was one such case.

The expanded scan snapshot used the arm64 release Ninja compilation database, all 225 native C/C++ translation
units, and the original compile flags with `-fsyntax-only`, `-Wpointer-to-int-cast`,
`-Wint-to-pointer-cast`, and `-Wshorten-64-to-32`. All translation units passed syntax checking. It
reported 251 pointer-to-integer, 57 integer-to-pointer, and 12 pointer-width narrowing diagnostics
(320 occurrences; 309 unique diagnostic locations). These are audit leads, not 320 confirmed bugs:
legacy pointer words, file-relative offsets, GPU packet links, and task handles need different
representations and must be classified at their consumers.

To reproduce the scan setup, configure the existing native build with
`cmake -S . -B build/macos-arm64_rel -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON`, then run the diagnostic
flags against each command in `build/macos-arm64_rel/compile_commands.json`, replacing object output
and compile/dependency options with `-fsyntax-only`.

## Resolved finding

### Dungeon relocation dereferenced 32-bit file pointers as host pointers

- The user-reported crash loaded the file at `0x110ad0d8`, then faulted at `0x100ad714` while
  writing `m->name` in `src/stag4000/dungfile.c:84`. The fault address is the relocated floor
  structure's low-32-bit address without the host region.
- `Stg40_RelocDungFile` now restores the relocated `Stg40DungFloorRel *` and each
  `Stg40DungLayoutRel *` through `P32` before dereferencing them.
- The loaded `floorMap` is also a legacy 32-bit pointer (`include/stag4000/stag4000.h:321`). Its
  direct casts in `dungfile.c`, `cellgrid.c`, `spawn.c`, and `stag4000.c` now use `P32` as well.
- Added `tests/dungfile_reloc_test.c`, which calls the production relocation function on a synthetic
  file buffer above 4 GB. Before the fix LLDB stopped at `EXC_BAD_ACCESS` on `0x10000020`; after
  the fix it prints `dungfile_reloc_test: PASS`.
- The arm64 release build succeeds after the fix (incremental build: 57 warnings across 6 files).
  A headless movie-enabled startup smoke completed 600 requested VBlank waits and exited normally.
- **Remaining validation:** the synthetic test verifies the real relocator and pointer restoration.
  The next user-driven dungeon-entry attempt progressed past relocation and exposed the flood-fill
  finding below; the route after both fixes still needs to be rerun.

### Room-label flood fill narrowed its allocated queue pointer to `s32`

- The user's next dungeon-entry crash faulted at `0x10136500` in the first `Stg40FillPt` queue
  write, with `buf=269706496` (`0x10136500`). `Mem_Alloc` returns `sptr`, but `Stg40_LabelRooms`
  stored its result in `s32` and `Stg40_FloodFillRoom` accepted `s32`, discarding the host address's
  upper bits.
- The queue is now a typed `Stg40FillPt *` from allocation through the function declaration and
  implementation; queue reads and writes use that pointer directly.
- Added `tests/floodfill_pointer_test.c`, which calls the production flood fill with a one-cell grid
  and a queue mapped above 4 GB. Before the fix, LLDB showed `buf=0x10000000` and `EXC_BAD_ACCESS`
  on the first write; after the fix it prints `floodfill_pointer_test: PASS`.
- The full arm64 release build and both focused dungeon tests pass. The full user-driven
  dungeon-entry route still needs to be rerun.

### Random-hazard scratch buffer was narrowed before `Mem_Free`

- The user's next dungeon crash stopped in `Mem_Free` with `arg0=0x100dace8` and faulted at
  `0x100dacd0`, the low-32-bit alias of the allocation's header. `Stg40_SpawnRandomHazards` stored
  `Mem_Alloc`'s `sptr` result in `s32 buf`, then converted that truncated value back to pointers for
  `Stg40_SpawnHazardAtRandom` and `Mem_Free`.
- The local is now `u8 (*buf)[2]`; the allocation is converted once from `sptr` to this pointer type,
  and both the hazard helper and `Mem_Free` receive the full-width pointer.
- Added `tests/spawn_hazard_free_test.c`, which calls the production `Stg40_SpawnRandomHazards` with
  the production allocator and a heap above 4 GB. Before the fix LLDB reproduced `EXC_BAD_ACCESS`
  in `Mem_Free` at `0x10010000` from `arg0=0x10010018`; after the fix it prints
  `spawn_hazard_free_test: PASS` and confirms the allocation coalesced back into the heap.
- The arm64 release build and all three focused dungeon pointer tests pass. The full user-driven
  dungeon-entry route still needs to be rerun.

### Floor rendering narrowed the packet cursor to `s32`

- The user's next dungeon-entry trace stopped in `Stg40_DrawTileTop` while writing the flat floor
  packet: `pkt=269507596` (`0x10105c0c`), with the fault at `0x10105c13`. `Sys_State.packet.addr`
  is `sptr`, but `Stg40_DrawFloorTiles` copied it into `s32` and both tile helpers took/returned
  `s32`, discarding the host address's upper bits.
- The cursor now remains a `u8 *` through `Stg40_DrawFloorTiles`, `Stg40_DrawTileTop`, and
  `Stg40_DrawTileWalls`. OT link words are converted explicitly with `P32_SET`, retaining the
  existing low-24-bit link encoding where required.
- Added `tests/floor_packet_cursor_test.c`, which calls the production tile-top helper with a packet
  buffer above 4 GB and checks the flat packet write, cursor advance, and OT link. Before the fix,
  LLDB showed `pkt=0x10000000` and `EXC_BAD_ACCESS` at `0x10000007`; after the fix it prints
  `floor_packet_cursor_test: PASS`.
- The arm64 release build and all four focused dungeon pointer tests pass. The full user-driven
  dungeon-entry route still needs to be rerun.

## Deferred candidates and planned fixes

These findings are recorded for a later implementation pass. Preserve fixed-width PS1/file/GPU data
layouts; only native temporaries and native-only state should become pointer-sized. Every planned fix
must retain the PS1 32-bit behavior and add a regression test where a focused seam is available.

### A. High-confidence pointer-sized value truncations

| Candidate | Evidence | Planned fix and validation |
| --- | --- | --- |
| Side-model cached file pointer — `src/stag4000/obj.c:208-219,253-291` | `CdCacheEntry.data` is `sptr`, but `Stg40_SideModelFor` stores it in `s32 file` and the native-only side-model cache record. | Change the native-only cache member and local to `sptr` or a typed pointer; keep `fileId` separate. Validate cache hit/reload identity and side-model rendering with the cache above 4 GB. |
| Map background smoothing range — `src/stag2000/mapbg.c:127,159-162,203-218` | `layerStart` is `s32`, assigned from `Sys_State.packet.addr`, then converted through `u32` to pointers passed to `Host_SmoothLayer`. | Retain the start/end cursors as pointer-width values and pass them as pointers without the `u32` round-trip. Keep the OT packet-link conversions explicit. Test smoothing with a packet buffer above 4 GB and verify the range endpoints. |
| Stag0000 dungeon relocation — `src/stag0000/dungsel.c:23-50,126,403` | `Stg00_RelocDungFile` narrows the loaded file base to `u32`, then dereferences relocated `u32` words as host pointers (`h = (Stg00DungFloor *)*arg0`); related fields are later restored inconsistently. This is the same file-pointer pattern as the fixed STAG4000 crash. | Keep the loaded file base as `uptr`; resolve file-relative offsets against that host base and encode only the 32-bit file words with `P32_SET`. Add a synthetic >4 GB test for the production STAG0000 relocator and test the dungeon-selection route. |

### B. Fixed-width text and file pointer words

These public values/fields are legacy `s32` words consumed by text or data-file code. The planned
fix is explicit pointer arithmetic followed by `P32_SET` at the fixed-width boundary, paired with
`P32`/`Text_GetPointer` restoration at consumption—not widening the stored fields blindly.

| Candidate | Evidence | Planned fix and validation |
| --- | --- | --- |
| Flag-table state and branch result — `src/main/flagtable.c:71-107`; `FlagEntryState.fileBase` at `include/main/156C.h:113-121` | Three `sptr`→`s32` sites: `fileBase` assignment, `Flag_SelectBranch`'s text result, and `Flag_GetTableBase` return. Branch results flow to `Text_OpenMsgClearChoice`; `fileBase` has no C read site. | Trace all consumers/layout users. For the branch text word, add the offset at pointer width and encode with `P32_SET`. Remove the dead `fileBase` store if confirmed unused, otherwise preserve the 4-byte field as an explicit encoded word. Widen `Flag_GetTableBase` only if callers need a native pointer; current C call sites ignore its result. Test text lookup through the flag-event route. |
| Item text addresses — `src/main/gamedata.c:205-212` | `Item_GetNameText` and `Item_GetDescText` add file offsets to `Cd_GetFileOrNull` (`sptr`) then return the result as `s32`; these values feed `Text_OpenPacked`, message arguments, and `Text_GetPointer`. | Keep pointer arithmetic pointer-width; encode only where the legacy text-reference API requires a 32-bit word, and verify every native consumer restores it. Add a high-address item name/description test. |
| Skill text addresses — `src/main/skill.c:57-68` | Same `sptr`→`s32` text-reference conversion for skill names/descriptions. | Apply the same explicit text-reference encode/decode path and test name/description rendering from a high-address asset. |
| Warp/map destination label — `src/stag2000/warppad.c:38-47`; fixed `Stg20FileRec.text` at `include/stag2000/stag2000.h:272-279` | File base is narrowed to `s32` before adding the text offset, then stored in a fixed-width record field consumed by `Text_OpenPacked`. | Resolve the file-relative offset at pointer width and store via `P32_SET`, leaving the 0x18-byte record unchanged. Validate map-label display after relocation. |

### C. Fixed-width pointer round-trips and legacy call boundaries

| Candidate family | Evidence | Planned fix and validation |
| --- | --- | --- |
| Digimon-transfer records — `src/main/digilist.c:54,68,78,81,112`; `MenuDigiPickRow.record` and `MenuCtx.transferRecord` in `include/main/156C.h:1234,1762-1769` | Native pointers are stored/retrieved through fixed 4-byte `s32` fields. | Keep the established record layouts; encode stores with `P32_SET` and restore reads with `P32`. Add a transfer/swap regression above 4 GB and exercise the menu route. |
| Actor/entity pointers cast to `s32` throughout menus and overlays | Pointer-cast diagnostics cluster in `main/digilist.c`, `digistatus.c`, `itemmenu.c`, `itemuse.c`, `nameentry.c`, `skilllist.c`, `status.c`, `submenu.c`, `topmenu.c`, and multiple `stag0000`–`stag4000` overlay files. Before the fix, `Menu_MoveGridCursor` accepted `s32` cursor/grid arguments; other examples include pointer-valued `a3` arguments in `Item_ApplyToDigi` / `Item_UseStatBoost` at `src/main/itemeffect.c:185-225`. | `Menu_MoveGridCursor` and `Menu_MoveGridCursorP1` now take typed `s16 *` cursor/grid arguments; their callers no longer narrow those pointers to `s32`. `tests/topmenu_cursor_pointer_test.c` allocates both structures above 4 GB and verifies cursor movement; LLDB observed the pre-fix dereference at the truncated `0x10000020`, and the regression passes after the fix. The other actor/entity pointer-cast sites in this family remain deferred; trace and fix them individually rather than replacing casts mechanically. |
| GPU ordering-table and packet links | Pointer-to-`u32` warnings occur in `main/anim.c`, `fade.c`, `model.c`, `ot.c`, `parts.c`, `portrait.c`, `text.c`, and overlay renderers. The consumer often expects a 24-bit PS1 OT link rather than a host pointer. | Classify each field by protocol. Preserve fixed-width GPU words and encode through a named low-24-bit/link helper only after validating the packet arena invariant; keep packet cursors themselves pointer-width. Add link tests for the helper and retain arena checks. |
| File-relative/packed asset pointers | Casts occur in `main/model.c`, `main/anim.c`, `stag0000/dungsel.c`, and related asset consumers. Some values are offsets, not pointers. | Keep serialized offsets 32-bit and resolve them against their owning file base through typed accessors; use `P32` only for actual host pointer words. Add relocation/accessor tests with a file buffer above 4 GB. |
| Heap bookkeeping and pointer differences | Pointer-to-`s32` warnings include `main/mem.c`, `main/sys.c`, and `main/primbuf.c`. Some compare addresses within the constrained PS1 heap/arena. | Verify each arithmetic operation's same-region invariant. Use `sptr` for host address arithmetic/comparisons, keeping true byte counts and legacy heap words 32-bit. Add allocator boundary tests and keep the arena-region assertion. |

### D. Intentional representations to preserve

- `Text_GetPointer` restores legacy text words; preserve the 32-bit text-reference interface unless
  all callers and storage are migrated together.
- `P32`/`P32_SET` represent raw pointer words within the 4 GB region containing `Ps1_Ram`; these are
  not file-relative offsets.
- Task references are generation-tagged handles, not truncated pointers; retain `Task_GetActor` /
  `Task_GetRef` semantics.
- `Gpu_AllocPacketBufs` stores the packet-buffer byte count in `Gpu_PrimBufs[2]` as an integer-shaped
  sentinel (`src/main/primbuf.c:43-46`); do not “fix” this size value as if it were a pointer.
- OT link words are fixed-width hardware encodings. Keep the packet cursor pointer-width and narrow
  only at the explicit link-writing boundary.

The explicit pointer-cast warnings are grouped above by semantic family, not certified safe one by
one. The actor/entity, GPU-link, file-offset, and heap groups are deferred candidate inventories; the
implementation pass must trace each listed expression to its actual consumer before classifying it.

## Planned implementation order

1. Fix the remaining high-confidence pointer truncations in `obj.c`, `mapbg.c`, and
   `stag0000/dungsel.c`, with focused high-address tests and dungeon/map runtime validation.
2. Resolve legacy text/file-reference sites in `flagtable.c`, `gamedata.c`, `skill.c`, and
   `warppad.c`; verify their fixed-width consumers before changing representation.
3. Fix the Digimon-transfer record round-trip with `P32_SET`/`P32` and a transfer-flow regression.
4. Triage the remaining pointer-cast inventory by actor/task, GPU-link, file-offset, and heap-arithmetic
   families; resolve each native pointer-valued call chain and document intentional conversions.
5. Add a repeatable diagnostic gate for the compile-database scan, with reviewed exceptions for
   intentional fixed-width encodings, then run release build, focused tests, PS1 layout/build checks,
   and the user-driven dungeon route.

## Known intentional matches / false-positive examples

- Text pointers are stored in legacy `s32` words and restored by `Text_GetPointer` in
  `src/main/text.c:112-122`.
- Texture-slot references are stored as legacy pointer words and restored in
  `src/main/texslot.c:186-195`.
- GPU packet links and file-relative offsets have their own 32-bit representations; inspect their
  consumer before treating a cast as a defect.

## Current status

The dungeon relocation, room-label flood-fill, random-hazard allocation, and floor packet cursor fixes
and focused regressions are verified. The expanded static audit is recorded above; remaining candidate
fixes are deferred. The actual dungeon route still needs validation after the remaining pointer-width
candidates are addressed.
