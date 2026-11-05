#include <stdio.h>

#include "common.h"
#include "main/156C.h"
#include "main/itemextension.h"

static ItemTableEntry vanillaItems[] = {
    { .u0.field_0 = 0x020300FE },
    { .u0.field_0 = 0x030400FF },
    { .u0.id = 0 },
};

EntA0 *Cd_GetFileEntry(arg0)
s32 arg0;
{
    return (EntA0 *)(arg0 == 0x45E0000 ? vanillaItems : NULL);
}

sptr Cd_GetFileOrNull(s32 key) {
    (void)key;
    return 0;
}

s32 Item_GetNameText(s32 id);
s32 Item_GetDescText(s32 id);
s32 Item_GetCategory(s32 id);
s32 Item_GetLevel(s32 id);
s32 Item_CheckId(s32 id);
s32 Item_GetIdAtIndex(s32 index);
s32 Item_GetTableIndex(s32 id);

static int check(int ok, const char *name) {
    if (!ok) {
        printf("item_extension_test: FAIL (%s)\n", name);
        return 1;
    }
    return 0;
}

int main(void) {
    const ItemExtension *item;
    const u8 *name;
    const u8 *description;
    GameState state = { 0 };
    ItemExtensionInventorySave inventory, restored;
    u8 inventory_bytes[ITEM_EXTENSION_SAVE_MAX_BYTES];
    u16 inventory_length;
    s32 id;
    int failures = 0;

    ItemExtension_Init();
    item = ItemExtension_FindByKey(0x494E464Du);
    failures += check(item != NULL, "source item is registered by stable key");
    failures += check(item != NULL && item->runtimeId == 0xFD, "runtime ID avoids vanilla IDs");

    id = item != NULL ? item->runtimeId : 0;
    failures += check(ItemExtension_FindById(id) == item, "runtime ID resolves to source item");
    failures += check(Item_GetCategory(id) == 0x21, "InfMissile uses Wave Missile category");
    failures += check(Item_GetLevel(id) == 5, "InfMissile has maximum hazard tier");
    failures += check(!ItemExtension_ConsumesOnUse(id), "InfMissile has infinite uses");
    failures += check(ItemExtension_ConsumesOnUse(0xFE), "vanilla items keep their default consume behavior");
    failures += check(Item_CheckId(id) == 0, "source item passes item ID validation");
    failures += check(Item_GetIdAtIndex(2) == id, "source item follows vanilla table entries");
    failures += check(Item_GetTableIndex(id) == 2, "source item has a catalog sort index");

    name = ItemExtension_GetTextPointer(Item_GetNameText(id));
    description = ItemExtension_GetTextPointer(Item_GetDescText(id));
    failures += check(name != NULL && name[0] == 0x12 && name[1] == 0x31 && name[2] == 0x29,
                      "source name is encoded for the retail text renderer");
    failures += check(description != NULL && description[0] == 0x0D && description[1] == 0x28,
                      "source description is encoded for the retail text renderer");
    failures += check(ItemExtension_GetTextPointer(0x12345678) == NULL, "unrelated text handles pass through");

    state.bagItems[0] = 0xFE;
    state.bagItems[1] = (u16)id;
    state.bagItems[2] = 0xFF;
    state.storageCounts[id] = 3;
    state.slotItems[11] = (u16)id;
    ItemExtension_CaptureInventory(&state, &inventory);
    failures += check(inventory.bagCount == 1 && inventory.bag[0].key == 0x494E464Du &&
                          inventory.bag[0].position == 1,
                      "capture separates custom bag entries from vanilla state");
    failures += check(state.bagItems[0] == 0xFE && state.bagItems[1] == 0xFF && state.bagItems[2] == 0,
                      "capture compacts vanilla bag entries");
    failures += check(state.storageCounts[id] == 0 && state.slotItems[11] == 0,
                      "capture removes custom counts and equipment from vanilla state");

    inventory_length = ItemExtension_SerializeInventory(&inventory, inventory_bytes, sizeof(inventory_bytes));
    failures += check(inventory_length != 0 &&
                          ItemExtension_DeserializeInventory(&restored, inventory_bytes, inventory_length),
                      "custom inventory round-trips in the extension format");
    ItemExtension_RestoreInventory(&state, &restored);
    failures += check(state.bagItems[0] == 0xFE && state.bagItems[1] == id && state.bagItems[2] == 0xFF,
                      "restore merges custom items back into their original bag positions");
    failures += check(state.storageCounts[id] == 3 && state.slotItems[11] == id,
                      "restore rehydrates custom storage and equipment references");

    if (failures == 0) {
        puts("item_extension_test: PASS");
        return 0;
    }
    return 1;
}
