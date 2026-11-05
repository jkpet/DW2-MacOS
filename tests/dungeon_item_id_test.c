#include <stdio.h>

#include "stag4000/itemselection.h"

int main(void) {
    u8 legacy_ids[0x30] = { 0 };

    Stg40_ItemSelectionReset();
    Stg40_ItemSelectionSet(legacy_ids, 0, 0x7FFF);
    Stg40_ItemSelectionSet(legacy_ids, 1, 0xFE);
    if (legacy_ids[0] != 0xFF) {
        puts("dungeon_item_id_test: FAIL (legacy menu byte remains compatible)");
        return 1;
    }
    if (Stg40_ItemSelectionGet(0) != 0x7FFF) {
        printf("dungeon_item_id_test: FAIL (selected ID truncated to 0x%X)\n",
               (unsigned)Stg40_ItemSelectionGet(0));
        return 1;
    }
    if (Stg40_ItemSelectionGet(1) != 0xFE) {
        puts("dungeon_item_id_test: FAIL (vanilla ID mapping changed)");
        return 1;
    }
    Stg40_ItemSelectionSetSelected(&legacy_ids[2], Stg40_ItemSelectionGet(0));
    if (legacy_ids[2] != 0xFF || Stg40_ItemSelectionGetSelected() != 0x7FFF) {
        puts("dungeon_item_id_test: FAIL (selected item ID was truncated)");
        return 1;
    }
    Stg40_ItemSelectionReset();
    if (Stg40_ItemSelectionGet(0) != 0 || Stg40_ItemSelectionGet(1) != 0 ||
        Stg40_ItemSelectionGetSelected() != 0) {
        puts("dungeon_item_id_test: FAIL (reset leaves stale selections)");
        return 1;
    }
    puts("dungeon_item_id_test: PASS");
    return 0;
}
