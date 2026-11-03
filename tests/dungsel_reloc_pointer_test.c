#include "common.h"
#include "stag0000/stag0000.h"
#include "stag0000/dungsel.h"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_FILE_SIZE 0x1000
#define FLOOR_OFFSET 0x100
#define LAYOUT_OFFSET 0x200
#define NAME_OFFSET 0x300
#define CMD_OFFSET 0x500

u_char *Ps1_Ram;
PadState Pad_State[2];

static int fail(const char *message) {
    fprintf(stderr, "dungsel_reloc_pointer_test: %s\n", message);
    return 1;
}

void Task_SetState1(Actor *actor, u32 state) {
    (void)actor;
    (void)state;
}

int main(void) {
    u8 *file = mmap((void *)(uptr)0x110000000, TEST_FILE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
    u32 *floorTable;
    Stg00DungFloor *floor;
    Stg00DungLayout *layout;
    Stg00RelocCmd *cmd;
    Stg00SelWork work = { 0 };
    u32 base;
    s32 count;
    s32 i;
    pid_t child;
    int status;

    if (file == MAP_FAILED) {
        return fail("could not allocate test dungeon file");
    }
    if ((uptr)file <= UINT32_MAX) {
        munmap(file, TEST_FILE_SIZE);
        return fail("test requires a dungeon file above the 32-bit address range");
    }

    Ps1_Ram = file;
    base = (u32)(uptr)file;
    floorTable = (u32 *)file;
    floorTable[0] = FLOOR_OFFSET;
    floorTable[1] = 0;

    floor = (Stg00DungFloor *)(file + FLOOR_OFFSET);
    floor->name = NAME_OFFSET;
    for (i = 0; i < 8; i++) {
        floor->layouts[i] = LAYOUT_OFFSET;
    }

    layout = (Stg00DungLayout *)(file + LAYOUT_OFFSET);
    layout->cellBits = 0x400;
    layout->spawnPoints = 0x404;
    layout->chests = 0x408;
    layout->cmdList = CMD_OFFSET;
    layout->enemyParties = 0x410;
    cmd = (Stg00RelocCmd *)(file + CMD_OFFSET);
    cmd->posPicks01.tag = 0xFF;

    child = fork();
    if (child == -1) {
        munmap(file, TEST_FILE_SIZE);
        return fail("could not launch the relocation repro");
    }
    if (child == 0) {
        count = Stg00_RelocDungFile(floorTable);

        if (count != 1 || floorTable[0] != base + FLOOR_OFFSET) {
            _exit(2);
        }
        if (floor->name != base + NAME_OFFSET) {
            _exit(3);
        }
        for (i = 0; i < 8; i++) {
            if (floor->layouts[i] != base + LAYOUT_OFFSET) {
                _exit(4);
            }
        }
        if (layout->cellBits != base + 0x400 || layout->spawnPoints != base + 0x404 ||
            layout->chests != base + 0x408 || layout->cmdList != base + CMD_OFFSET ||
            layout->enemyParties != base + 0x410) {
            _exit(5);
        }

        work.floorCount = 1;
        work.lastFloor = -1;
        work.floorTable = floorTable;
        Stg00_DungSelPickFloor(NULL, &work);
        _exit(work.lastFloor == 0 ? 0 : 6);
    }

    if (waitpid(child, &status, 0) != child) {
        munmap(file, TEST_FILE_SIZE);
        return fail("could not collect the relocation repro");
    }
    munmap(file, TEST_FILE_SIZE);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return fail("relocation or floor selection faulted on a high-address dungeon file");
    }
    puts("dungsel_reloc_pointer_test: PASS");
    return 0;
}
