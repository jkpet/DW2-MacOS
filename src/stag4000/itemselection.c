#include "stag4000/itemselection.h"

static u16 sItemIds[0x30];
static u16 sSelectedItemId;

void Stg40_ItemSelectionReset(void) {
    s32 i;
    for (i = 0; i < 0x30; i++) {
        sItemIds[i] = 0;
    }
    sSelectedItemId = 0;
}

void Stg40_ItemSelectionSet(u8 *legacyIds, s32 index, s32 itemId) {
    if (legacyIds == NULL || index < 0 || index >= 0x30) {
        return;
    }
    legacyIds[index] = (u8)itemId;
    sItemIds[index] = (u16)itemId;
}

s32 Stg40_ItemSelectionGet(s32 index) {
    if (index < 0 || index >= 0x30) {
        return 0;
    }
    return sItemIds[index];
}

void Stg40_ItemSelectionSetSelected(u8 *legacyId, s32 itemId) {
    if (legacyId == NULL) {
        return;
    }
    *legacyId = (u8)itemId;
    sSelectedItemId = (u16)itemId;
}

s32 Stg40_ItemSelectionGetSelected(void) {
    return sSelectedItemId;
}
