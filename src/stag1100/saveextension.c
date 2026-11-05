#include <string.h>

#include "stag1100/saveextension.h"

static u32 extension_capacity(void) {
    u32 used = (u32)sizeof(Stg11SaveList) + (u32)sizeof(Stg11SaveExtensionHeader);
    return used < STG11_SAVE_DATA_BYTES ? STG11_SAVE_DATA_BYTES - used : 0;
}

static int chunk_stream_valid(const u8 *data, u32 length) {
    u32 offset = 0;

    while (offset < length) {
        Stg11SaveExtensionChunk chunk;
        u32 chunk_bytes;

        if (length - offset < sizeof(chunk)) {
            return 0;
        }
        memcpy(&chunk, data + offset, sizeof(chunk));
        chunk_bytes = (u32)sizeof(chunk) + chunk.length;
        if (chunk_bytes > length - offset) {
            return 0;
        }
        offset += chunk_bytes;
    }
    return offset == length;
}

int Stg11_SaveExtensionInit(u8 *saveData) {
    Stg11SaveExtensionHeader *header;
    u32 capacity = extension_capacity() / STG11_SAVE_EXTENSION_SLOTS;
    u32 base;
    s32 i;

    if (saveData == NULL || capacity < sizeof(Stg11SaveExtensionChunk)) {
        return 0;
    }
    base = (u32)sizeof(Stg11SaveList);
    memset(saveData + base, 0, STG11_SAVE_DATA_BYTES - base);
    header = (Stg11SaveExtensionHeader *)(saveData + base);
    header->magic = STG11_SAVE_EXTENSION_MAGIC;
    header->version = STG11_SAVE_EXTENSION_VERSION;
    header->headerBytes = sizeof(*header);
    header->slotCapacity = capacity;
    header->totalBytes = sizeof(*header) + capacity * STG11_SAVE_EXTENSION_SLOTS;
    for (i = 0; i < STG11_SAVE_EXTENSION_SLOTS; i++) {
        header->slotOffset[i] = sizeof(*header) + capacity * i;
        header->slotLength[i] = 0;
    }
    return 1;
}

int Stg11_SaveExtensionValidate(const u8 *saveData) {
    const Stg11SaveExtensionHeader *header;
    u32 base = (u32)sizeof(Stg11SaveList);
    u32 available = STG11_SAVE_DATA_BYTES - base;
    s32 i;

    if (saveData == NULL || available < sizeof(*header)) {
        return 0;
    }
    header = (const Stg11SaveExtensionHeader *)(saveData + base);
    if (header->magic != STG11_SAVE_EXTENSION_MAGIC || header->version != STG11_SAVE_EXTENSION_VERSION ||
        header->headerBytes != sizeof(*header) || header->headerBytes > available || header->slotCapacity == 0 ||
        header->slotCapacity > (available - header->headerBytes) / STG11_SAVE_EXTENSION_SLOTS ||
        header->totalBytes != header->headerBytes + header->slotCapacity * STG11_SAVE_EXTENSION_SLOTS) {
        return 0;
    }
    for (i = 0; i < STG11_SAVE_EXTENSION_SLOTS; i++) {
        u32 expected_offset = header->headerBytes + header->slotCapacity * (u32)i;
        if (header->slotOffset[i] != expected_offset ||
            header->slotLength[i] > header->slotCapacity ||
            !chunk_stream_valid(saveData + base + header->slotOffset[i], header->slotLength[i])) {
            return 0;
        }
    }
    return 1;
}

u8 *Stg11_SaveExtensionSlot(u8 *saveData, s32 slot, u32 *capacity, u32 *used) {
    Stg11SaveExtensionHeader *header;
    u32 base = (u32)sizeof(Stg11SaveList);

    if (slot < 0 || slot >= STG11_SAVE_EXTENSION_SLOTS || !Stg11_SaveExtensionValidate(saveData)) {
        return NULL;
    }
    header = (Stg11SaveExtensionHeader *)(saveData + base);
    if (capacity != NULL) {
        *capacity = header->slotCapacity;
    }
    if (used != NULL) {
        *used = header->slotLength[slot];
    }
    return saveData + base + header->slotOffset[slot];
}

const u8 *Stg11_SaveExtensionFindChunk(const u8 *slotData, u32 used, u32 type, u16 version,
                                      const void **payload, u16 *length) {
    u32 offset = 0;

    if (slotData == NULL || !chunk_stream_valid(slotData, used)) {
        return NULL;
    }
    while (offset < used) {
        Stg11SaveExtensionChunk chunk;
        memcpy(&chunk, slotData + offset, sizeof(chunk));
        if (chunk.type == type && chunk.version == version) {
            if (payload != NULL) {
                *payload = slotData + offset + sizeof(chunk);
            }
            if (length != NULL) {
                *length = chunk.length;
            }
            return slotData + offset;
        }
        offset += sizeof(chunk) + chunk.length;
    }
    return NULL;
}

int Stg11_SaveExtensionSetChunk(u8 *saveData, s32 slot, u32 type, u16 version,
                                const void *payload, u16 length) {
    Stg11SaveExtensionHeader *header;
    u8 *slotData;
    u32 capacity, used, offset = 0;
    u32 newBytes = (u32)sizeof(Stg11SaveExtensionChunk) + length;
    u32 base = (u32)sizeof(Stg11SaveList);

    slotData = Stg11_SaveExtensionSlot(saveData, slot, &capacity, &used);
    if (slotData == NULL || (length != 0 && payload == NULL)) {
        return 0;
    }
    while (offset < used) {
        Stg11SaveExtensionChunk chunk;
        u32 oldBytes;
        memcpy(&chunk, slotData + offset, sizeof(chunk));
        oldBytes = (u32)sizeof(chunk) + chunk.length;
        if (chunk.type == type) {
            u32 tailOffset = offset + oldBytes;
            u32 tailBytes = used - tailOffset;
            if (newBytes > oldBytes && newBytes - oldBytes > capacity - used) {
                return 0;
            }
            memmove(slotData + offset + newBytes, slotData + tailOffset, tailBytes);
            used = used - oldBytes + newBytes;
            break;
        }
        offset += oldBytes;
    }
    if (offset == used && newBytes > capacity - used) {
        return 0;
    }
    {
        Stg11SaveExtensionChunk chunk;
        chunk.type = type;
        chunk.version = version;
        chunk.length = length;
        memcpy(slotData + offset, &chunk, sizeof(chunk));
        if (length != 0) {
            memcpy(slotData + offset + sizeof(chunk), payload, length);
        }
    }

    header = (Stg11SaveExtensionHeader *)(saveData + base);
    header->slotLength[slot] = used + (offset == used ? newBytes : 0);
    if (offset < used) {
        /* Replacement preserves the shifted tail and only changes the net used length. */
        u32 stream = 0;
        while (stream < header->slotLength[slot]) {
            Stg11SaveExtensionChunk chunk;
            memcpy(&chunk, slotData + stream, sizeof(chunk));
            stream += sizeof(chunk) + chunk.length;
        }
        header->slotLength[slot] = stream;
    }
    return 1;
}
