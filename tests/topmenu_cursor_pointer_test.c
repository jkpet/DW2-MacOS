#include "common.h"
#include "main/topmenu.h"

#include <sys/mman.h>

PadState Pad_State[2];

int main(void) {
    s16 *grid;
    s16 *cursor;

    grid = mmap((void *)(uptr)0x110000000, 0x1000, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON, -1, 0);
    if (grid == MAP_FAILED) {
        return 1;
    }
    if ((uptr)grid <= UINT32_MAX) {
        munmap(grid, 0x1000);
        return 2;
    }

    cursor = grid + 16;
    grid[0] = 2;
    grid[1] = 2;
    cursor[0] = 0;
    cursor[1] = 0;

    Pad_State[0].repeat = 0x2000;
    if (Menu_MoveGridCursorP1(cursor, grid) != -1 || cursor[0] != 1 || cursor[1] != 0) {
        munmap(grid, 0x1000);
        return 3;
    }

    munmap(grid, 0x1000);
    (void)write(1, (u8 *)"topmenu_cursor_pointer_test: PASS\n",
                sizeof("topmenu_cursor_pointer_test: PASS\n") - 1);
    return 0;
}
