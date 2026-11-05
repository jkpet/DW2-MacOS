#ifndef STAG1100_SAVEEXTENSION_H
#define STAG1100_SAVEEXTENSION_H

#include "common.h"
#include "stag1100/stag1100.h"

#define STG11_SAVE_EXTENSION_MAGIC 0x58453244u /* "D2EX" */
#define STG11_SAVE_EXTENSION_VERSION 1
#define STG11_SAVE_EXTENSION_SLOTS 3

typedef struct {
    u32 magic;
    u16 version;
    u16 headerBytes;
    u32 totalBytes;
    u32 slotOffset[STG11_SAVE_EXTENSION_SLOTS];
    u32 slotCapacity;
    u32 slotLength[STG11_SAVE_EXTENSION_SLOTS];
} Stg11SaveExtensionHeader;

typedef struct {
    u32 type;
    u16 version;
    u16 length;
} Stg11SaveExtensionChunk;

/* The extension area is stored after the unchanged retail save-list structure in card data. */
int Stg11_SaveExtensionInit(u8 *saveData);
int Stg11_SaveExtensionValidate(const u8 *saveData);
u8 *Stg11_SaveExtensionSlot(u8 *saveData, s32 slot, u32 *capacity, u32 *used);
const u8 *Stg11_SaveExtensionFindChunk(const u8 *slotData, u32 used, u32 type, u16 version,
                                      const void **payload, u16 *length);
int Stg11_SaveExtensionSetChunk(u8 *saveData, s32 slot, u32 type, u16 version,
                                const void *payload, u16 length);

#endif /* STAG1100_SAVEEXTENSION_H */
