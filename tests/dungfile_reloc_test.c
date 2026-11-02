#include "common.h"
#include "stag4000/stag4000.h"
#include "stag4000/dungfile.h"

#include <stdio.h>
#include <sys/mman.h>

#define TEST_FILE_SIZE 0x1000
#define FLOOR_OFFSET 0x20
#define LAYOUT_OFFSET 0x80
#define NAME_OFFSET 0x100

u_char *Ps1_Ram;

static int fail(const char *message) {
    fprintf(stderr, "dungfile_reloc_test: %s\n", message);
    return 1;
}

int main(void) {
    u8 *file;
    u32 *floorTable;
    Stg40DungFloorRel *floor;
    Stg40DungLayoutRel *layout;
    u32 base;
    s32 count;
    s32 i;
    s32 j;

    file = mmap((void *)(uptr)0x110000000, TEST_FILE_SIZE, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON, -1, 0);
    if (file == MAP_FAILED) {
        return fail("could not allocate test file buffer");
    }
    if ((uptr)file <= UINT32_MAX) {
        munmap(file, TEST_FILE_SIZE);
        return fail("test requires a file buffer above the 32-bit address range");
    }

    Ps1_Ram = file;
    base = (u32)(uptr)file;
    floorTable = (u32 *)file;
    floorTable[0] = FLOOR_OFFSET;
    floorTable[1] = 0;

    floor = (Stg40DungFloorRel *)(file + FLOOR_OFFSET);
    floor->name = NAME_OFFSET;
    for (i = 0; i < 8; i++) {
        floor->layouts[i] = LAYOUT_OFFSET;
    }

    layout = (Stg40DungLayoutRel *)(file + LAYOUT_OFFSET);
    for (i = 0; i < 5; i++) {
        layout->offsets[i] = 0x200 + (u32)i * 4;
    }

    count = Stg40_RelocDungFile((s32 *)file);
    if (count != 1) {
        munmap(file, TEST_FILE_SIZE);
        return fail("expected exactly one floor-table entry");
    }
    if (floorTable[0] != base + FLOOR_OFFSET) {
        munmap(file, TEST_FILE_SIZE);
        return fail("floor-table entry was not relocated");
    }
    if (floor->name != base + NAME_OFFSET) {
        munmap(file, TEST_FILE_SIZE);
        return fail("floor name pointer was not relocated");
    }
    for (i = 0; i < 8; i++) {
        if (floor->layouts[i] != base + LAYOUT_OFFSET) {
            munmap(file, TEST_FILE_SIZE);
            return fail("floor layout pointer was not relocated");
        }
    }
    for (j = 0; j < 5; j++) {
        if (layout->offsets[j] != base + 0x200 + (u32)j * 4) {
            munmap(file, TEST_FILE_SIZE);
            return fail("layout offset was not relocated");
        }
    }

    munmap(file, TEST_FILE_SIZE);
    puts("dungfile_reloc_test: PASS");
    return 0;
}
