#include <stdio.h>
#include <string.h>

#include "stag1100/saveextension.h"

int main(void) {
    u8 data[STG11_SAVE_DATA_BYTES] = { 0 };
    u8 first[32], second[8];
    u32 capacity, used;
    const void *payload;
    const u8 *slot;
    u16 length;

    ((Stg11SaveList *)data)->used[0] = 0x12345678;
    if (!Stg11_SaveExtensionInit(data) || ((Stg11SaveList *)data)->used[0] != 0x12345678 ||
        !Stg11_SaveExtensionValidate(data)) {
        puts("save_extension_test: FAIL (init and preserve vanilla prefix)");
        return 1;
    }
    memset(first, 0xA5, sizeof(first));
    memset(second, 0x5A, sizeof(second));
    if (!Stg11_SaveExtensionSetChunk(data, 0, 0x54455354, 1, first, sizeof(first)) ||
        !Stg11_SaveExtensionSetChunk(data, 1, 0x54455354, 1, second, sizeof(second))) {
        puts("save_extension_test: FAIL (write slot chunks)");
        return 1;
    }
    slot = Stg11_SaveExtensionSlot(data, 0, &capacity, &used);
    if (slot == NULL || used != sizeof(Stg11SaveExtensionChunk) + sizeof(first) || capacity < used ||
        Stg11_SaveExtensionFindChunk(slot, used, 0x54455354, 1, &payload, &length) == NULL ||
        length != sizeof(first) || memcmp(payload, first, sizeof(first)) != 0) {
        puts("save_extension_test: FAIL (read slot chunk)");
        return 1;
    }
    if (!Stg11_SaveExtensionSetChunk(data, 0, 0x54455354, 2, second, sizeof(second)) ||
        !Stg11_SaveExtensionValidate(data)) {
        puts("save_extension_test: FAIL (replace chunk with another version)");
        return 1;
    }
    slot = Stg11_SaveExtensionSlot(data, 1, NULL, &used);
    if (Stg11_SaveExtensionFindChunk(slot, used, 0x54455354, 1, &payload, &length) == NULL ||
        length != sizeof(second) || memcmp(payload, second, sizeof(second)) != 0) {
        puts("save_extension_test: FAIL (slot data isolation)");
        return 1;
    }

    data[sizeof(Stg11SaveList) + sizeof(Stg11SaveExtensionHeader) + 6] = 0xFF;
    data[sizeof(Stg11SaveList) + sizeof(Stg11SaveExtensionHeader) + 7] = 0xFF;
    if (Stg11_SaveExtensionValidate(data)) {
        puts("save_extension_test: FAIL (reject malformed chunk stream)");
        return 1;
    }
    puts("save_extension_test: PASS");
    return 0;
}
