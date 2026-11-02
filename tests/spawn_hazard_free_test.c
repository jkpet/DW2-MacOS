#include "common.h"
#include "stag4000/stag4000.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define TEST_ARENA_SIZE 0x20000
#define FLOOR_OFFSET 0x8000
#define HEAP_OFFSET 0x10000
#define HEAP_SIZE 0x10000

u_char *Ps1_Ram;
Stg40B60 *Stg40_RootState;
DungState *Dung_StatePtr;

extern void Mem_InitHeap(MemBlock *heap, s32 size);
extern void Stg40_SpawnRandomHazards(void);

static int fail(const char *message) {
    fprintf(stderr, "spawn_hazard_free_test: %s\n", message);
    return 1;
}

s32 Stg40_RandInt(s32 n) {
    (void)n;
    return 0;
}

void Stg40_SetCellOccupied(s32 x, s32 y, s32 flag) {
    (void)x;
    (void)y;
    (void)flag;
}

void Cd_EvictLruFile(void) {}

int main(void) {
    u8 *arena;
    Stg40B60 rootState = { 0 };
    Stg40DungFloor *floor;
    MemBlock *heap;
    MemBlock *scratchBlock;
    sptr guardAllocation;

    arena = mmap((void *)(uptr)0x110000000, TEST_ARENA_SIZE, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANON, -1, 0);
    if (arena == MAP_FAILED) {
        return fail("could not allocate test arena");
    }
    if ((uptr)arena <= UINT32_MAX) {
        munmap(arena, TEST_ARENA_SIZE);
        return fail("test requires an arena above the 32-bit address range");
    }

    Ps1_Ram = arena;
    floor = (Stg40DungFloor *)(arena + FLOOR_OFFSET);
    memset(floor, 0, sizeof(*floor));
    rootState.roomCount = 1;
    rootState.floorMap = P32_SET(floor);
    Stg40_RootState = &rootState;

    heap = (MemBlock *)(arena + HEAP_OFFSET);
    Mem_InitHeap(heap, HEAP_SIZE);
    guardAllocation = Mem_Alloc(0x100, 2);
    if (guardAllocation == 0) {
        munmap(arena, TEST_ARENA_SIZE);
        return fail("could not allocate guard block");
    }
    Stg40_SpawnRandomHazards();

    scratchBlock = heap->next;
    if (heap->tag != 2 || scratchBlock->tag != 0 || scratchBlock->prev != heap ||
        scratchBlock->next->tag != 1 || scratchBlock->next->prev != scratchBlock) {
        munmap(arena, TEST_ARENA_SIZE);
        return fail("hazard scratch allocation was not freed and coalesced");
    }

    munmap(arena, TEST_ARENA_SIZE);
    puts("spawn_hazard_free_test: PASS");
    return 0;
}
