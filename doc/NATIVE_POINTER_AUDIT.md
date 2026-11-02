# Native Pointer-Width Audit Candidates

Findings from the macOS arm64 native build. The dungeon relocation crash was reproduced from the
user's trace and fixed with a focused regression test. The other listed candidates still need
runtime investigation.

## Audit method

The initial scan used the release build's Ninja compilation database and Clang's pointer-size
diagnostics (`-Wpointer-to-int-cast` and `-Wint-to-pointer-cast`) with the existing arm64 compile
flags, in `-fsyntax-only` mode. Checked 163 C translation units under `src/`; all passed syntax
checking.
The pass emitted 253 pointer-to-integer and 73 integer-to-pointer diagnostic occurrences across
70 files. These are diagnostic occurrences, not unique bugs: intentional PS1 ABI representations,
GPU packet links, and known recovery paths account for many matches.

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
- **Remaining validation:** the synthetic test verifies the real relocator and pointer restoration;
  the full user-driven dungeon-entry route has not yet been rerun.

## Open candidates

### 1. Dungeon floor rendering narrows the packet cursor to `s32`

- `Sys_State.packet.addr` is `sptr` in the native layout (`include/main/156C.h:2284-2286`).
- `src/stag4000/floor.c:615-633` copies it into `s32 pkt`, passes that value through
  `Stg40_DrawTileTop` / `Stg40_DrawTileWalls` (whose cursor parameters are also `s32`), then writes
  it back to `packet.addr`.
- Those helpers cast `pkt` directly to packet pointers when writing at `floor.c:550-562` and
  `floor.c:581-609`. On arm64, the narrowing can discard the host address's upper bits before the
  write.
- **Follow-up:** run a dungeon-floor render under LLDB and compare `packet.addr` with `pkt` at the
  first helper call.

### 2. Room-label flood fill narrows a heap allocation to `s32`

- `Mem_Alloc` returns `sptr` (`src/main/mem.c:143-148`), which is pointer-sized in the native
  build.
- `src/stag4000/cellgrid.c:247-258` stores that result in `s32 buf`, passes it to
  `Stg40_FloodFillRoom(s32 buf, ...)`, then casts it to `Stg40FillPt *` for writes/reads at
  `cellgrid.c:183-208` and to `ActorWork *` for `Mem_Free` at line 258.
- The allocation's upper address bits appear to be lost on native arm64.
- **Follow-up:** check the allocation and first flood-fill write while entering a dungeon with an
  unlabeled room; verify the buffer address survives each call boundary.

## Additional candidate

### 3. Digimon-transfer rows round-trip pointers through fixed-width record fields

- `MenuDigiPickRow.record` and `MenuCtx.transferRecord` are fixed `s32` fields
  (`include/main/156C.h:1234,1762-1769`).
- `src/main/digilist.c:54,68,78,81,112` stores and retrieves `DigiRosterSwapRec *` values through
  these fields, with direct integer-to-pointer casts on retrieval.
- This may be an intentional legacy pointer-word representation, but the retrieval sites shown
  should be checked for `P32`-style restoration on 64-bit builds.
- **Follow-up:** trace the transfer-pick flow and confirm the row/transfer record pointers refer to
  the same native addresses before and after storage.

## Known intentional matches / false-positive examples

- Text pointers are stored in legacy `s32` words and restored by `Text_GetPointer` in
  `src/main/text.c:112-122`.
- Texture-slot references are stored as legacy pointer words and restored in
  `src/main/texslot.c:186-195`.
- GPU packet links and file-relative offsets have their own 32-bit representations; inspect their
  consumer before treating a cast as a defect.

## Current status

The relocation fix and test are in the working tree. The open candidates above have not yet been
confirmed by runtime reproduction.
