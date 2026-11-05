# Source-defined item extensions

Added items are defined in `src/main/itemextension.c`; there is no runtime item-data sidecar.
Each definition has a stable key for saves and receives an available runtime ID after the retail
item table is loaded. Item lookup, list enumeration, effect-record lookup, and text resolution
consult the extension layer before falling back to the disc data.

Keep each key stable once it can appear in a save. Names and descriptions are source strings
converted to the retail text glyph encoding. The current conversion supports digits, English
letters, spaces, and common punctuation. A custom behavior must use an existing game behavior or
be implemented in source; the data definition alone does not add new engine behavior.

`InfMissile` is the first definition. It uses category `0x21` (Wave Missile) and level 5. Both
Electro-Spore and Rock obstacle requirements accept that category, and the dungeon compares item
level to obstacle tier. The existing missile-gun part requirement still applies. No acquisition
reward has been added yet; `giveItem("InfMissile", 1)` in the opt-in Lua console is the test hook.

## Save representation

The retail `GameState` and three-slot save-list layout remain unchanged. Custom bag, storage, and
equipped-item references are extracted from the runtime view and stored in per-slot, versioned
chunks after the retail save-list. Stable extension keys—not temporary runtime IDs—are persisted.
Loading merges the known extension inventory back into the runtime view.

Native saves use a four-block memory-card file (version `0x103`). The reader also accepts retail
two-block saves (version `0x102`), initializes empty extension chunks, and grows the file when it is
next saved. The card checksum covers both the unchanged retail data and extension area. Retail
data is kept separately so a future exporter can omit the extension and write a vanilla projection.
