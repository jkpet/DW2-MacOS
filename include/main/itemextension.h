#ifndef MAIN_ITEMEXTENSION_H
#define MAIN_ITEMEXTENSION_H

#include "common.h"
#include "main/156C.h"

#define ITEM_EXTENSION_SAVE_CHUNK_TYPE 0x54494D45u /* "EMIT" in little endian. */
#define ITEM_EXTENSION_SAVE_VERSION 1
#define ITEM_EXTENSION_SAVE_BAG_CAPACITY 0x30
#define ITEM_EXTENSION_SAVE_STORAGE_CAPACITY 0xFF
#define ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS 0x13
#define ITEM_EXTENSION_SAVE_MAX_BYTES \
    (4 + ITEM_EXTENSION_SAVE_BAG_CAPACITY * 5 + ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS * 4 + \
     ITEM_EXTENSION_SAVE_STORAGE_CAPACITY * 6)

typedef struct {
    u32 key;
    u8 position;
} ItemExtensionBagSaveEntry;

typedef struct {
    u32 key;
    u16 count;
} ItemExtensionStorageSaveEntry;

typedef struct {
    u8 bagCount;
    u8 storageCount;
    ItemExtensionBagSaveEntry bag[ITEM_EXTENSION_SAVE_BAG_CAPACITY];
    u32 equipped[ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS];
    ItemExtensionStorageSaveEntry storage[ITEM_EXTENSION_SAVE_STORAGE_CAPACITY];
} ItemExtensionInventorySave;

/* Source-defined items that extend, but do not replace, the disc item tables. */
typedef struct {
    u32 key;              /* Stable source/save identity; unlike runtimeId, never allocated. */
    u32 price;
    u8 category;
    u8 level;
    u8 bodyMask;
    u8 consumeOnUse;
    ItemEffect effect;
    const char *name;
    const char *description;
    s32 runtimeId;        /* Free retail-sized ID allocated at initialization. */
    ItemTableEntry tableEntry;
    u8 encodedName[48];
    u8 encodedDescription[192];
} ItemExtension;

void ItemExtension_Init(void);
const ItemExtension *ItemExtension_FindById(s32 id);
const ItemExtension *ItemExtension_FindByKey(u32 key);
u32 ItemExtension_GetKeyByName(const char *name);
int ItemExtension_ConsumesOnUse(s32 id);
const ItemExtension *ItemExtension_At(s32 index);
s32 ItemExtension_Count(void);
s32 ItemExtension_GetTextHandle(s32 id, s32 description);
const u8 *ItemExtension_GetTextPointer(s32 handle);
s32 ItemExtension_GetCatalogCount(void);
s32 ItemExtension_GetIdAtIndex(s32 index);
s32 ItemExtension_GetTableIndex(s32 id, s32 vanillaCount);
void ItemExtension_CaptureInventory(GameState *state, ItemExtensionInventorySave *save);
void ItemExtension_RestoreInventory(GameState *state, const ItemExtensionInventorySave *save);
u16 ItemExtension_SerializeInventory(const ItemExtensionInventorySave *save, u8 *bytes, u32 capacity);
int ItemExtension_DeserializeInventory(ItemExtensionInventorySave *save, const u8 *bytes, u32 length);

#endif /* MAIN_ITEMEXTENSION_H */
