#include "common.h"
#include "stag4000/stag4000.h"
#include "stag4000/cellgrid.h"

#include <stdio.h>
#include <sys/mman.h>

DungState *Dung_StatePtr;

static int fail(const char *message) {
    fprintf(stderr, "floodfill_pointer_test: %s\n", message);
    return 1;
}

int main(void) {
    DungState state = { 0 };
    Stg40FloorHeader floor = { 0 };
    Stg40Cell cell = { .flags = 0xC000 };
    Stg40FillPt *queue;

    queue = mmap((void *)(uptr)0x110000000, 0x1000, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANON, -1, 0);
    if (queue == MAP_FAILED) {
        return fail("could not allocate test queue");
    }
    if ((uptr)queue <= UINT32_MAX) {
        munmap(queue, 0x1000);
        return fail("test requires a queue above the 32-bit address range");
    }

    floor.cols = 1;
    floor.rows = 1;
    state.floorHdr = &floor;
    state.cells = (ActorWork *)&cell;
    Dung_StatePtr = &state;

    Stg40_FloodFillRoom(queue, 0, 0, 0, 0x2000);

    if (!(cell.flags & 0x2000)) {
        munmap(queue, 0x1000);
        return fail("flood fill did not mark the starting cell");
    }
    if (queue[0].x != 0 || queue[0].y != 0) {
        munmap(queue, 0x1000);
        return fail("flood fill did not preserve the starting point");
    }

    munmap(queue, 0x1000);
    puts("floodfill_pointer_test: PASS");
    return 0;
}
