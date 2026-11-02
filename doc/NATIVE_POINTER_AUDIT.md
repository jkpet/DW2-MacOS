# Native Pointer-Width Audit Candidates

Report-only findings from the macOS arm64 native build. These are candidates for future
investigation; none of the dungeon/menu paths below has yet been confirmed by a runtime
reproduction.

## Audit method

Used the release build's Ninja compilation database and Clang's pointer-size diagnostics
(`-Wpointer-to-int-cast` and `-Wint-to-pointer-cast`) with the existing arm64 compile flags, in
`-fsyntax-only` mode. Checked 163 C translation units under `src/`; all passed syntax checking.
The pass emitted 253 pointer-to-integer and 73 integer-to-pointer diagnostic occurrences across
70 files. These are diagnostic occurrences, not unique bugs: intentional PS1 ABI representations,
GPU packet links, and known recovery paths account for many matches.

## Higher-priority candidates

### 1. Dungeon `floorMap` is stored as a 32-bit relocated pointer, then directly cast back

- `include/stag4000/stag4000.h:321` declares `floorMap` as `s32`.
- `src/stag4000/dungfile.c:71-89` relocates file-relative values using a `u32` base. At line 26,
  `Stg40_LoadDungFile` stores an entry from that relocated table in `floorMap`.
- The value is directly cast to `Stg40DungFloor *` at `src/stag4000/dungfile.c:45`,
  `src/stag4000/cellgrid.c:71`, `src/stag4000/spawn.c:138,178,376`, and
  `src/stag4000/stag4000.c:125`.
- The same dungeon code uses `P32` for relocated members such as `m->layouts`, `m->name`, and
  `layout->spawnPoints`. That suggests `floorMap` may need the same native pointer restoration
  before dereference.
- **Follow-up:** exercise dungeon loading and check that the restored pointer falls inside the
  loaded dungeon file; inspect all `floorMap` reads for the appropriate typed accessor.

### 2. Dungeon floor rendering narrows the packet cursor to `s32`

- `Sys_State.packet.addr` is `sptr` in the native layout (`include/main/156C.h:2284-2286`).
- `src/stag4000/floor.c:615-633` copies it into `s32 pkt`, passes that value through
  `Stg40_DrawTileTop` / `Stg40_DrawTileWalls` (whose cursor parameters are also `s32`), then writes
  it back to `packet.addr`.
- Those helpers cast `pkt` directly to packet pointers when writing at `floor.c:550-562` and
  `floor.c:581-609`. On arm64, the narrowing can discard the host address's upper bits before the
  write.
- **Follow-up:** run a dungeon-floor render under LLDB and compare `packet.addr` with `pkt` at the
  first helper call.

### 3. Room-label flood fill narrows a heap allocation to `s32`

- `Mem_Alloc` returns `sptr` (`src/main/mem.c:143-148`), which is pointer-sized in the native
  build.
- `src/stag4000/cellgrid.c:247-258` stores that result in `s32 buf`, passes it to
  `Stg40_FloodFillRoom(s32 buf, ...)`, then casts it to `Stg40FillPt *` for writes/reads at
  `cellgrid.c:183-208` and to `ActorWork *` for `Mem_Free` at line 258.
- The allocation's upper address bits appear to be lost on native arm64.
- **Follow-up:** check the allocation and first flood-fill write while entering a dungeon with an
  unlabeled room; verify the buffer address survives each call boundary.

## Additional candidate

### 4. Digimon-transfer rows round-trip pointers through fixed-width record fields

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

No code changes were made as part of this audit. The strongest candidates above have not yet been
tested by a runtime route into the dungeon or transfer-pick flows.
