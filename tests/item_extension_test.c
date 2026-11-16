#include <stdio.h>

#include "common.h"
#include "main/156C.h"
#include "main/itemextension.h"
#include "stag4000/stag4000.h"

s32 Item_Use(s32 itemId, s32 bagSlot, s32 target, s32 record);
void Menu_UseBugZapItem(Actor *actor);
extern DungState *Dung_StatePtr;

static ItemTableEntry vanillaItems[] = {
    { .u0.field_0 = 0x020300FE },
    { .u0.field_0 = 0x030400FF },
    { .u0.id = 0 },
};
static s32 sLastCdFileEntryKey;

EntA0 *Cd_GetFileEntry(arg0)
s32 arg0;
{
    sLastCdFileEntryKey = arg0;
    return (EntA0 *)(arg0 == 0x45E0000 ? vanillaItems : NULL);
}

sptr Cd_GetFileOrNull(s32 key) {
    (void)key;
    return 0;
}

DungState Test_DungState;
DungState *Dung_StatePtr = &Test_DungState;
u_char *Ps1_Ram;
GameState *Save_GameStatePtr;
MenuCtx Test_MenuCtx;
MenuCtx *Menu_Ctx = &Test_MenuCtx;
static s32 sRemoveFromBagCalls;
static s32 sNoBugZapMessageCount;

void Text_OpenPacked(void *slot, s32 text, u32 style, Halves pos) {
    (void)slot;
    (void)text;
    (void)style;
    (void)pos;
    if (sLastCdFileEntryKey == 0x1FD00A0) {
        sNoBugZapMessageCount++;
    }
}

void Text_OpenDesc(void *slot, TextDesc *desc) {
    (void)slot;
    (void)desc;
}

void Text_Close(s32 *slot) {
    (void)slot;
}

s32 Bug_GetMaxMemBugLevel(void) {
    s32 best = 0;
    s32 i;
    for (i = 0; i < Test_DungState.status.memBugCount; i++) {
        if (best < Test_DungState.status.memBugLevels[i]) {
            best = Test_DungState.status.memBugLevels[i];
        }
    }
    return best;
}

s32 Beetle_GetPart(s32 part) {
    (void)part;
    return 0;
}

void Beetle_SetPartBroken(s32 part, s32 broken) {
    (void)part;
    (void)broken;
}

s32 Digi_AddExp(s32 exp, s32 amount) {
    return exp + amount;
}

u8 Digi_GetType(s32 digiId) {
    (void)digiId;
    return 0;
}

s32 Digi_ScaleExpGain(s32 amount) { return amount; }
s32 Rand_Next(void) { return 0; }

void Item_RemoveFromBag(s32 slot) {
    (void)slot;
    sRemoveFromBagCalls++;
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
    const ItemExtension *zap;
    const ItemExtension *beam;
    const u8 *name;
    const u8 *description;
    GameState state = { 0 };
    GameState zapState = { 0 };
    ItemExtensionInventorySave inventory, restored;
    u8 inventory_bytes[ITEM_EXTENSION_SAVE_MAX_BYTES];
    u16 inventory_length;
    s32 id;
    int failures = 0;

    ItemExtension_Init();
    item = ItemExtension_FindByKey(0x494E464Du);
    failures += check(item != NULL, "source item is registered by stable key");
    failures += check(item != NULL && item->runtimeId >= ITEM_EXTENSION_RUNTIME_ID_MIN &&
                          item->runtimeId <= ITEM_EXTENSION_RUNTIME_ID_MAX,
                      "runtime ID uses the extension range outside byte-sized vanilla storage");

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

    zap = ItemExtension_FindByKey(0x494E465Au);
    failures += check(zap != NULL, "InfZap is registered by its stable key");
    if (zap != NULL) {
        s32 zapId = zap->runtimeId;

        zapState.bagItems[0] = (u16)zapId;
        Test_DungState.status.bugLevels[0] = 1;
        Test_DungState.status.bugLevels[1] = 2;
        Test_DungState.status.bugLevels[2] = 3;
        Test_DungState.status.memBugCount = 3;
        Test_DungState.status.memBugLevels[0] = 1;
        Test_DungState.status.memBugLevels[1] = 2;
        Test_DungState.status.memBugLevels[2] = 3;
        sRemoveFromBagCalls = 0;

        failures += check(Item_Use(zapId, 0, 0, 0) == 1,
                          "InfZap clears all nest types and memory bugs through the universal zap effect");
        failures += check(Test_DungState.status.bugLevels[0] == 0 &&
                              Test_DungState.status.bugLevels[1] == 0 &&
                              Test_DungState.status.bugLevels[2] == 0 &&
                              Test_DungState.status.memBugCount == 0,
                          "InfZap clears every nest and memory-bug tier");
        failures += check(zapState.bagItems[0] == zapId && sRemoveFromBagCalls == 0,
                          "using InfZap leaves it in the bag");

        Test_DungState.status.bugLevels[0] = 3;
        Test_DungState.status.bugLevels[1] = 3;
        Test_DungState.status.bugLevels[2] = 3;
        Test_DungState.status.memBugCount = 1;
        Test_DungState.status.memBugLevels[0] = 3;
        failures += check(Item_Use(zapId, 0, 0, 0) == 1 &&
                              Test_DungState.status.bugLevels[0] == 0 &&
                              Test_DungState.status.bugLevels[1] == 0 &&
                              Test_DungState.status.bugLevels[2] == 0 &&
                              Test_DungState.status.memBugCount == 0 &&
                              zapState.bagItems[0] == zapId && sRemoveFromBagCalls == 0,
                          "InfZap can be reused to clear maximum-tier bugs");

        Test_MenuCtx.itemId = (s16)zapId;
        Test_MenuCtx.bagSlot = 0;
        {
            Actor actor = { 0 };
            MenuItemUseWork work = { 0 };
            actor.work = (ActorWork *)&work;
            sNoBugZapMessageCount = 0;
            Menu_UseBugZapItem(&actor);
            failures += check(sNoBugZapMessageCount == 1,
                              "the no-BugZap message appears when dungeon bug status is empty");
        }
        {
            s32 nestType;
            for (nestType = 0; nestType < 3; nestType++) {
                Actor actor = { 0 };
                MenuItemUseWork work = { 0 };
                Test_DungState.status.bugLevels[0] = 0;
                Test_DungState.status.bugLevels[1] = 0;
                Test_DungState.status.bugLevels[2] = 0;
                Test_DungState.status.memBugCount = 0;
                Test_DungState.status.bugLevels[nestType] = 3;
                actor.work = (ActorWork *)&work;
                sNoBugZapMessageCount = 0;
                Menu_UseBugZapItem(&actor);
                failures += check(Test_DungState.status.bugLevels[0] == 0 &&
                                      Test_DungState.status.bugLevels[1] == 0 &&
                                      Test_DungState.status.bugLevels[2] == 0 &&
                                      sNoBugZapMessageCount == 0,
                                  "InfZap clears each active nest through the menu without the no-BugZap message");
            }
        }
    }

    beam = ItemExtension_FindByKey(0x44494749u);
    failures += check(beam != NULL, "Digibeam is registered by its stable key");
    failures += check(sizeof(Stg40ItemReq) == 0x10,
                      "gift-category expansion preserves the dungeon request record size");
    if (beam != NULL) {
        s32 beamId = beam->runtimeId;
        s32 i;
        int categoryAllowed = 0;

        for (i = 0; i < 5; i++) {
            if (Stg40_GiftGunReq.itemCategories[i] == Item_GetCategory(beamId)) {
                categoryAllowed = 1;
            }
        }
        failures += check(Item_GetCategory(beamId) == 0x25,
                          "Digibeam uses the universal gift category");
        failures += check(Item_GetLevel(beamId) == 5,
                          "Digibeam awards the top gift-points tier");
        failures += check(!ItemExtension_ConsumesOnUse(beamId),
                          "Digibeam can be reused indefinitely");
        failures += check(categoryAllowed,
                          "Digibeam appears in the dungeon gift selection list");
    }

    state.bagItems[0] = 0xFE;
    state.bagItems[1] = (u16)id;
    state.bagItems[2] = 0xFF;
    ItemExtension_SetStorageCount(&state, id, 3);
    state.slotItems[11] = (u16)id;
    ItemExtension_CaptureInventory(&state, &inventory);
    failures += check(inventory.bagCount == 1 && inventory.bag[0].key == 0x494E464Du &&
                          inventory.bag[0].position == 1,
                      "capture separates custom bag entries from vanilla state");
    failures += check(state.bagItems[0] == 0xFE && state.bagItems[1] == 0xFF && state.bagItems[2] == 0,
                      "capture compacts vanilla bag entries");
    failures += check(ItemExtension_GetStorageCount(&state, id) == 3 && state.slotItems[11] == 0,
                      "capture keeps custom storage in the extension layer and removes custom equipment from vanilla state");

    inventory_length = ItemExtension_SerializeInventory(&inventory, inventory_bytes, sizeof(inventory_bytes));
    failures += check(inventory_length != 0 &&
                          ItemExtension_DeserializeInventory(&restored, inventory_bytes, inventory_length),
                      "custom inventory round-trips in the extension format");
    ItemExtension_RestoreInventory(&state, &restored);
    failures += check(state.bagItems[0] == 0xFE && state.bagItems[1] == id && state.bagItems[2] == 0xFF,
                      "restore merges custom items back into their original bag positions");
    failures += check(ItemExtension_GetStorageCount(&state, id) == 3 && state.slotItems[11] == id,
                      "restore rehydrates custom storage and equipment references");

    if (failures == 0) {
        puts("item_extension_test: PASS");
        return 0;
    }
    return 1;
}
