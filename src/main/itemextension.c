#include <string.h>

#include "main/itemextension.h"

#ifdef DW2_NATIVE
extern ItemTableEntry *Item_FindById(s32 id);
#else
extern ItemTableEntry *Item_FindById();
#endif

#define ITEM_EXTENSION_TEXT_TAG 0x7F000000u
#define ITEM_EXTENSION_TEXT_TAG_MASK 0xFF000000u

/* Source data uses stable keys. Runtime IDs are assigned only after checking the loaded retail
 * table, so extensions do not reserve or steal a retail ID. */
static ItemExtension extensions[] = {
    {
        .key = 0x494E464Du, /* "INFM" */
        .price = 0,
        .category = 0x21, /* Wave missile: accepted by Electro-Spore and Rock requirements. */
        .level = 5,       /* Dungeon obstacle tiers top out at level 5. */
        .bodyMask = 0,
        .consumeOnUse = 0,
        .effect = { 0, 0, 0 },
        .name = "InfMissile",
        .description = "Destroys all tiers of Electro-Spore and Rock.",
        .runtimeId = 0,
        .tableEntry = { 0 },
        .encodedName = { 0 },
        .encodedDescription = { 0 },
    },
};

static int initialized;

static u8 encode_glyph(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return (u8)(c - '0');
    }
    if (c >= 'A' && c <= 'Z') {
        return (u8)(0x0A + c - 'A');
    }
    if (c >= 'a' && c <= 'z') {
        return (u8)(0x24 + c - 'a');
    }
    switch (c) {
    case ' ':
        return 0xFD;
    case '-':
        return 0x49;
    case '?':
        return 0x44;
    case '!':
        return 0x45;
    case '/':
        return 0x46;
    case ',':
        return 0x54;
    case '.':
        return 0x55;
    case '\'':
        return 0x56;
    case '"':
        return 0x57;
    case ';':
        return 0x58;
    case ':':
        return 0x59;
    case '%':
        return 0x5A;
    case '+':
        return 0x5B;
    case '=':
        return 0x5C;
    case '#':
        return 0x5D;
    default:
        return 0x44;
    }
}

static void encode_text(u8 *dst, size_t capacity, const char *src) {
    size_t i = 0;

    while (src != NULL && *src != 0 && i + 1 < capacity) {
        dst[i++] = encode_glyph((unsigned char)*src++);
    }
    dst[i] = 0xFF;
}

void ItemExtension_Init(void) {
    s32 candidate;
    s32 i;

    if (initialized) {
        return;
    }
    initialized = 1;

    for (i = 0; i < (s32)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        ItemExtension *item = &extensions[i];

        for (candidate = ITEM_EXTENSION_RUNTIME_ID_MAX; candidate >= ITEM_EXTENSION_RUNTIME_ID_MIN; candidate--) {
            s32 j;
            int already_assigned = 0;

            if (Item_FindById(candidate) != NULL) {
                continue;
            }
            for (j = 0; j < i; j++) {
                if (extensions[j].runtimeId == candidate) {
                    already_assigned = 1;
                    break;
                }
            }
            if (!already_assigned) {
                item->runtimeId = candidate;
                break;
            }
        }

        if (item->runtimeId == 0) {
            continue;
        }
        item->tableEntry.u0.id = (s16)item->runtimeId;
        item->tableEntry.u0.b0.category = item->category;
        item->tableEntry.u0.b0.field_3 = item->level;
        item->tableEntry.u4.b4.bodyMask = item->bodyMask;
        item->tableEntry.nameOffset = ItemExtension_GetTextHandle(item->runtimeId, 0);
        item->tableEntry.descOffset = ItemExtension_GetTextHandle(item->runtimeId, 1);
        encode_text(item->encodedName, sizeof(item->encodedName), item->name);
        encode_text(item->encodedDescription, sizeof(item->encodedDescription), item->description);
    }
}

const ItemExtension *ItemExtension_FindById(s32 id) {
    s32 i;

    ItemExtension_Init();
    for (i = 0; i < (s32)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        if (extensions[i].runtimeId != 0 && extensions[i].runtimeId == id) {
            return &extensions[i];
        }
    }
    return NULL;
}

const ItemExtension *ItemExtension_FindByKey(u32 key) {
    s32 i;

    ItemExtension_Init();
    for (i = 0; i < (s32)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        if (extensions[i].key == key) {
            return &extensions[i];
        }
    }
    return NULL;
}

u32 ItemExtension_GetKeyByName(const char *name) {
    s32 i;

    if (name == NULL) {
        return 0;
    }
    for (i = 0; i < (s32)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        if (strcmp(extensions[i].name, name) == 0) {
            return extensions[i].key;
        }
    }
    return 0;
}

int ItemExtension_ConsumesOnUse(s32 id) {
    const ItemExtension *item = ItemExtension_FindById(id);
    return item == NULL || item->consumeOnUse != 0;
}

u16 ItemExtension_GetStorageCount(GameState *state, s32 id) {
    const ItemExtension *item = ItemExtension_FindById(id);

    if (item != NULL) {
        return item->storedCount;
    }
    if (state == NULL || id < 0 || id >= 0x100) {
        return 0;
    }
    return state->storageCounts[id];
}

void ItemExtension_SetStorageCount(GameState *state, s32 id, u16 count) {
    s32 i;

    for (i = 0; i < ItemExtension_Count(); i++) {
        ItemExtension *item = (ItemExtension *)ItemExtension_At(i);
        if (item != NULL && item->runtimeId == id) {
            item->storedCount = count;
            return;
        }
    }
    if (state != NULL && id >= 0 && id < 0x100) {
        state->storageCounts[id] = count;
    }
}

const ItemExtension *ItemExtension_At(s32 index) {
    s32 i, found = 0;

    ItemExtension_Init();
    for (i = 0; i < (s32)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        if (extensions[i].runtimeId == 0) {
            continue;
        }
        if (found++ == index) {
            return &extensions[i];
        }
    }
    return NULL;
}

s32 ItemExtension_Count(void) {
    s32 i, count = 0;

    ItemExtension_Init();
    for (i = 0; i < (s32)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        count += extensions[i].runtimeId != 0;
    }
    return count;
}

s32 ItemExtension_GetTextHandle(s32 id, s32 description) {
    return (s32)(ITEM_EXTENSION_TEXT_TAG | (((u32)id & 0x7FFFFFu) << 1) | (description != 0));
}

const u8 *ItemExtension_GetTextPointer(s32 handle) {
    u32 value = (u32)handle;
    const ItemExtension *item;
    s32 id;

    if ((value & ITEM_EXTENSION_TEXT_TAG_MASK) != ITEM_EXTENSION_TEXT_TAG) {
        return NULL;
    }
    id = (s32)((value >> 1) & 0x7FFFFFu);
    item = ItemExtension_FindById(id);
    if (item == NULL) {
        return NULL;
    }
    return (value & 1u) ? item->encodedDescription : item->encodedName;
}

s32 ItemExtension_GetCatalogCount(void) {
    return ItemExtension_Count();
}

s32 ItemExtension_GetIdAtIndex(s32 index) {
    const ItemExtension *item = ItemExtension_At(index);
    return item != NULL ? item->runtimeId : 0;
}

s32 ItemExtension_GetTableIndex(s32 id, s32 vanillaCount) {
    s32 i;

    for (i = 0; i < ItemExtension_Count(); i++) {
        const ItemExtension *item = ItemExtension_At(i);
        if (item != NULL && item->runtimeId == id) {
            return vanillaCount + i;
        }
    }
    return -1;
}

static void write_u16(u8 *bytes, u16 value) {
    bytes[0] = (u8)value;
    bytes[1] = (u8)(value >> 8);
}

static void write_u32(u8 *bytes, u32 value) {
    bytes[0] = (u8)value;
    bytes[1] = (u8)(value >> 8);
    bytes[2] = (u8)(value >> 16);
    bytes[3] = (u8)(value >> 24);
}

static u16 read_u16(const u8 *bytes) {
    return (u16)(bytes[0] | ((u16)bytes[1] << 8));
}

static u32 read_u32(const u8 *bytes) {
    return (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

void ItemExtension_CaptureInventory(GameState *state, ItemExtensionInventorySave *save) {
    u16 vanilla_bag[ITEM_EXTENSION_SAVE_BAG_CAPACITY];
    s32 vanilla_count = 0;
    s32 i;

    if (state == NULL || save == NULL) {
        return;
    }
    memset(save, 0, sizeof(*save));

    for (i = 0; i < ITEM_EXTENSION_SAVE_BAG_CAPACITY; i++) {
        s32 id = state->bagItems[i];
        const ItemExtension *extension = ItemExtension_FindById(id);
        if (extension != NULL) {
            ItemExtensionBagSaveEntry *entry = &save->bag[save->bagCount++];
            entry->key = extension->key;
            entry->position = (u8)i;
        } else if (id != 0) {
            vanilla_bag[vanilla_count++] = (u16)id;
        }
    }
    memset(state->bagItems, 0, sizeof(state->bagItems));
    memcpy(state->bagItems, vanilla_bag, (size_t)vanilla_count * sizeof(vanilla_bag[0]));

    for (i = 0; i < ItemExtension_Count(); i++) {
        const ItemExtension *extension = ItemExtension_At(i);
        if (extension != NULL && extension->storedCount != 0 &&
            save->storageCount < ITEM_EXTENSION_SAVE_STORAGE_CAPACITY) {
            ItemExtensionStorageSaveEntry *entry = &save->storage[save->storageCount++];
            entry->key = extension->key;
            entry->count = extension->storedCount;
        }
    }

    for (i = 0; i < ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS; i++) {
        const ItemExtension *extension = ItemExtension_FindById(state->slotItems[i]);
        if (extension != NULL) {
            save->equipped[i] = extension->key;
            state->slotItems[i] = 0;
        }
    }
}

void ItemExtension_RestoreInventory(GameState *state, const ItemExtensionInventorySave *save) {
    u16 vanilla_bag[ITEM_EXTENSION_SAVE_BAG_CAPACITY];
    s32 vanilla_count = 0;
    s32 vanilla_index = 0;
    s32 i, j, k;

    if (state == NULL || save == NULL) {
        return;
    }
    for (i = 0; i < ItemExtension_Count(); i++) {
        ItemExtension *extension = (ItemExtension *)ItemExtension_At(i);
        if (extension != NULL) {
            extension->storedCount = 0;
        }
    }
    for (i = 0; i < ITEM_EXTENSION_SAVE_BAG_CAPACITY; i++) {
        if (state->bagItems[i] != 0) {
            vanilla_bag[vanilla_count++] = state->bagItems[i];
        }
    }
    memset(state->bagItems, 0, sizeof(state->bagItems));
    i = 0;
    for (j = 0; j < ITEM_EXTENSION_SAVE_BAG_CAPACITY && i < ITEM_EXTENSION_SAVE_BAG_CAPACITY; j++) {
        const ItemExtensionBagSaveEntry *bag_entry = NULL;
        const ItemExtension *extension;

        for (k = 0; k < save->bagCount; k++) {
            if (save->bag[k].position == j) {
                bag_entry = &save->bag[k];
                break;
            }
        }
        if (bag_entry != NULL) {
            extension = ItemExtension_FindByKey(bag_entry->key);
            if (extension != NULL) {
                state->bagItems[i++] = (u16)extension->runtimeId;
            }
        } else if (vanilla_index < vanilla_count) {
            state->bagItems[i++] = vanilla_bag[vanilla_index++];
        }
    }

    for (i = 0; i < save->storageCount; i++) {
        const ItemExtension *extension = ItemExtension_FindByKey(save->storage[i].key);
        if (extension != NULL) {
            ItemExtension_SetStorageCount(state, extension->runtimeId, save->storage[i].count);
        }
    }
    for (i = 0; i < ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS; i++) {
        const ItemExtension *extension = ItemExtension_FindByKey(save->equipped[i]);
        if (extension != NULL) {
            state->slotItems[i] = (u16)extension->runtimeId;
        }
    }
}

u16 ItemExtension_SerializeInventory(const ItemExtensionInventorySave *save, u8 *bytes, u32 capacity) {
    u32 length;
    u32 offset = 4;
    s32 i;

    if (save == NULL || bytes == NULL || save->bagCount > ITEM_EXTENSION_SAVE_BAG_CAPACITY ||
        save->storageCount > ITEM_EXTENSION_SAVE_STORAGE_CAPACITY) {
        return 0;
    }
    length = 4 + (u32)save->bagCount * 5 + ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS * 4 +
             (u32)save->storageCount * 6;
    if (length > capacity || length > 0xFFFF) {
        return 0;
    }
    write_u16(bytes, ITEM_EXTENSION_SAVE_VERSION);
    bytes[2] = save->bagCount;
    bytes[3] = save->storageCount;
    for (i = 0; i < save->bagCount; i++) {
        write_u32(bytes + offset, save->bag[i].key);
        bytes[offset + 4] = save->bag[i].position;
        offset += 5;
    }
    for (i = 0; i < ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS; i++) {
        write_u32(bytes + offset, save->equipped[i]);
        offset += 4;
    }
    for (i = 0; i < save->storageCount; i++) {
        write_u32(bytes + offset, save->storage[i].key);
        write_u16(bytes + offset + 4, save->storage[i].count);
        offset += 6;
    }
    return (u16)length;
}

int ItemExtension_DeserializeInventory(ItemExtensionInventorySave *save, const u8 *bytes, u32 length) {
    u32 expected, offset = 4;
    s32 i;

    if (save == NULL || bytes == NULL || length < 4 || read_u16(bytes) != ITEM_EXTENSION_SAVE_VERSION ||
        bytes[2] > ITEM_EXTENSION_SAVE_BAG_CAPACITY || bytes[3] > ITEM_EXTENSION_SAVE_STORAGE_CAPACITY) {
        return 0;
    }
    expected = 4 + (u32)bytes[2] * 5 + ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS * 4 + (u32)bytes[3] * 6;
    if (length != expected) {
        return 0;
    }
    memset(save, 0, sizeof(*save));
    save->bagCount = bytes[2];
    save->storageCount = bytes[3];
    for (i = 0; i < save->bagCount; i++) {
        s32 j;
        save->bag[i].key = read_u32(bytes + offset);
        save->bag[i].position = bytes[offset + 4];
        if (save->bag[i].key == 0 || save->bag[i].position >= ITEM_EXTENSION_SAVE_BAG_CAPACITY) {
            return 0;
        }
        for (j = 0; j < i; j++) {
            if (save->bag[j].position == save->bag[i].position) {
                return 0;
            }
        }
        offset += 5;
    }
    for (i = 0; i < ITEM_EXTENSION_SAVE_EQUIPMENT_SLOTS; i++) {
        save->equipped[i] = read_u32(bytes + offset);
        offset += 4;
    }
    for (i = 0; i < save->storageCount; i++) {
        s32 j;
        save->storage[i].key = read_u32(bytes + offset);
        save->storage[i].count = read_u16(bytes + offset + 4);
        if (save->storage[i].key == 0 || save->storage[i].count == 0) {
            return 0;
        }
        for (j = 0; j < i; j++) {
            if (save->storage[j].key == save->storage[i].key) {
                return 0;
            }
        }
        offset += 6;
    }
    return 1;
}
