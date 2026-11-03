#include "common.h"
#include "stag2000/stag2000.h"
#include "stag2000/walker.h"

#include <stdio.h>
#include <sys/mman.h>

#define TEST_BUFFER_SIZE 0x1000

SysState Sys_State;
Halves Gfx_NeutralRgb;

static Stg20MapFile sMapInfo;
static const void *sSmoothStart;
static const void *sSmoothEnd;

void Stg20_MapBgDraw(Actor *actor);

Actor *Stg20_FindWalkerByDigiId(s32 id) {
    (void)id;
    return NULL;
}

Stg20MapFile *Stg20_GetMapInfo(void) { return &sMapInfo; }

GfxTexSlot *Gfx_FindOrLoadTexSlot(s32 id) {
    (void)id;
    return NULL;
}

void Gfx_DrawParts(void *parts) { (void)parts; }
void SetGeomOffset(s32 x, s32 y) {
    (void)x;
    (void)y;
}

s32 Actor_ProjectToScreen(ContC40 *actor) {
    (void)actor;
    return 0;
}

int Host_WideMargin(int width) {
    (void)width;
    return 0;
}

EntA0 *Cd_GetFileEntry(u32 id) {
    (void)id;
    return NULL;
}

s32 Math_CycleRange(s32 value, s32 div, s32 lo, s32 hi) {
    (void)div;
    (void)lo;
    (void)hi;
    return value;
}

void Host_SmoothLayer(int id, const void *start, const void *end, int x, int y) {
    (void)id;
    (void)x;
    (void)y;
    sSmoothStart = start;
    sSmoothEnd = end;
}

static int fail(const char *message) {
    fprintf(stderr, "mapbg_smoothing_pointer_test: %s\n", message);
    return 1;
}

int main(void) {
    u8 *buffer = mmap((void *)(uptr)0x110000000, TEST_BUFFER_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    Stg20LoadWork work = { 0 };
    Actor actor = { 0 };
    u8 *packetStart;

    if (buffer == MAP_FAILED) {
        return fail("could not allocate packet buffer");
    }
    if ((uptr)buffer <= UINT32_MAX) {
        munmap(buffer, TEST_BUFFER_SIZE);
        return fail("test requires a packet buffer above the 32-bit address range");
    }

    packetStart = buffer + 0x100;
    actor.work = (ActorWork *)&work;
    Sys_State.packet.addr = (sptr)packetStart;
    sMapInfo.overlayParts = 0;
    Stg20_MapBgDraw(&actor);

    if (sSmoothStart != packetStart || sSmoothEnd != packetStart) {
        munmap(buffer, TEST_BUFFER_SIZE);
        return fail("smoothing layer received a truncated packet range");
    }

    munmap(buffer, TEST_BUFFER_SIZE);
    puts("mapbg_smoothing_pointer_test: PASS");
    return 0;
}
