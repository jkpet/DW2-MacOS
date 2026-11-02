#include "common.h"
#include "stag4000/stag4000.h"
#include "stag4000/floor.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

SysState Sys_State;

static int fail(const char *message) {
    fprintf(stderr, "floor_packet_cursor_test: %s\n", message);
    return 1;
}

int main(void) {
    Stg40FloorWork work = { 0 };
    Stg40F4 *packet;
    u32 ot = 0xAB123456;
    uptr next;

    packet = mmap((void *)(uptr)0x110000000, 0x1000, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANON, -1, 0);
    if (packet == MAP_FAILED) {
        return fail("could not allocate test packet buffer");
    }
    if ((uptr)packet <= UINT32_MAX) {
        munmap(packet, 0x1000);
        return fail("test requires a packet buffer above the 32-bit address range");
    }

    work.verts[0][0].s[1].flag = 1;
    Sys_State.otLayers.u[3] = &ot;

    next = (uptr)Stg40_DrawTileTop(&work, (u8 *)packet, 0, 0);

    if (next != (uptr)packet + sizeof(*packet)) {
        munmap(packet, 0x1000);
        return fail("floor packet cursor did not advance by one flat tile packet");
    }
    if (packet->tag.b.len != 5 || packet->code != 0x28) {
        munmap(packet, 0x1000);
        return fail("floor helper did not write the flat tile packet");
    }
    if (ot != (0xAB000000 | ((u32)(uptr)packet & 0xFFFFFF))) {
        munmap(packet, 0x1000);
        return fail("OT link did not preserve its high byte and encode the low 24-bit address");
    }

    munmap(packet, 0x1000);
    puts("floor_packet_cursor_test: PASS");
    return 0;
}
