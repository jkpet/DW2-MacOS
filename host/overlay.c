#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "host/host.h"

#ifdef _WIN32
/* Overlay data ranges (PE). CMakeLists.txt relinks each overlay so its .data and .bss are the grouped
 * sections .data$dw2ovl_<unit>_1 and _2; the markers below are _0 and _3. The linker sorts
 * .data$* by name, so [start, end) holds exactly that overlay's .data and .bss. */
#define OVL_MARKERS(unit)                                                                \
    __attribute__((section(".data$dw2ovl_" #unit "_0"), aligned(16))) u8 Ovl_DataStart_##unit[1] = { 0 }; \
    __attribute__((section(".data$dw2ovl_" #unit "_3"))) u8 Ovl_DataEnd_##unit[1] = { 0 };
#define OVL_START(unit) Ovl_DataStart_##unit
#define OVL_END(unit) Ovl_DataEnd_##unit
#elif defined(__APPLE__)
/* Overlay data ranges (Mach-O). CMakeLists.txt relinks each overlay so its __data and __bss are
 * one private __DATA section; the linker defines section$start$... and section$end$... symbols. */
#define OVL_MARKERS(unit, sect)                                              \
    extern u8 Ovl_DataStart_##unit[] __asm("section$start$__DATA$" sect);    \
    extern u8 Ovl_DataEnd_##unit[] __asm("section$end$__DATA$" sect);
#define OVL_START(unit) Ovl_DataStart_##unit
#define OVL_END(unit) Ovl_DataEnd_##unit
#else
/* Overlay data ranges (ELF). CMakeLists.txt relinks each overlay so its .data and .bss are one
 * section dw2ovl_<unit>; the linker defines __start_ / __stop_ around a section with a C name. */
#define OVL_MARKERS(unit) extern u8 __start_dw2ovl_##unit[], __stop_dw2ovl_##unit[];
#define OVL_START(unit) __start_dw2ovl_##unit
#define OVL_END(unit) __stop_dw2ovl_##unit
#endif

#if defined(__APPLE__)
OVL_MARKERS(stag0000, "__dw2ovl0")
OVL_MARKERS(stag1000, "__dw2ovl1")
OVL_MARKERS(stag1100, "__dw2ovl2")
OVL_MARKERS(stag2000, "__dw2ovl3")
OVL_MARKERS(stag3000, "__dw2ovl4")
OVL_MARKERS(stag3500, "__dw2ovl5")
OVL_MARKERS(stag4000, "__dw2ovl6")
#else
OVL_MARKERS(stag0000)
OVL_MARKERS(stag1000)
OVL_MARKERS(stag1100)
OVL_MARKERS(stag2000)
OVL_MARKERS(stag3000)
OVL_MARKERS(stag3500)
OVL_MARKERS(stag4000)
#endif

/* Boot image: the exe's initial bytes of the retail overlay area (Ovl_LoadArea), a TIM at +4
 * that Sys_Main shows at boot. Extracted by splat (assets/Ovl_LoadArea.bin). */
INCLUDE_BIN(Ovl_LoadArea, "assets/Ovl_LoadArea.bin");

typedef struct {
    const char *name;
    u8 *start;
    u8 *end;
    u8 *pristine;
} OvlRange;

/* Indexed by overlay id (Ovl_FileIds order). */
#define OVL_RANGE(unit) { #unit, OVL_START(unit), OVL_END(unit), 0 }
static OvlRange ovl_ranges[7] = {
    OVL_RANGE(stag0000), OVL_RANGE(stag4000), OVL_RANGE(stag2000), OVL_RANGE(stag1000),
    OVL_RANGE(stag3000), OVL_RANGE(stag1100), OVL_RANGE(stag3500),
};

static void check_ovl_ranges(void) {
    int i, j;

    for (i = 0; i < 7; i++) {
        const OvlRange *a = &ovl_ranges[i];

        if (a->end <= a->start) {
            fprintf(stderr, "[host] overlay %s: bad data range %p..%p\n", a->name, (void *)a->start, (void *)a->end);
            exit(1);
        }
    }
    for (i = 0; i < 7; i++) {
        const OvlRange *a = &ovl_ranges[i];

        for (j = i + 1; j < 7; j++) {
            const OvlRange *b = &ovl_ranges[j];
            u8 *start = a->start > b->start ? a->start : b->start;
            u8 *end = a->end < b->end ? a->end : b->end;

            if (start < end) {
                fprintf(stderr, "[host] overlay ranges overlap: %s %p..%p and %s %p..%p\n", a->name, (void *)a->start,
                        (void *)a->end, b->name, (void *)b->start, (void *)b->end);
                exit(1);
            }
        }
    }
}

/* Before any game code runs: keep the initial bytes of every overlay range. */
void Host_OvlSnapshot(void) {
    int i;

    check_ovl_ranges();
#ifdef __APPLE__
    printf("[host] overlay layout source: Mach-O section$start/section$end markers\n");
#endif
    for (i = 0; i < 7; i++) {
        OvlRange *r = &ovl_ranges[i];
        size_t n = (size_t)(r->end - r->start);

        r->pristine = malloc(n);
        if (r->pristine == NULL) {
            fprintf(stderr, "[host] overlay %s: out of memory for 0x%X bytes\n", r->name, (unsigned)n);
            exit(1);
        }
        memcpy(r->pristine, r->start, n);
        printf("[host] overlay %d %s: data+bss %p..%p (0x%X bytes)\n", i, r->name, (void *)r->start, (void *)r->end,
               (unsigned)n);
    }
}

void Host_OvlReset(int id) {
    OvlRange *r;

    if (id < 0 || id >= 7) {
        fprintf(stderr, "[host] Ovl_Load: bad overlay id %d\n", id);
        exit(1);
    }
    r = &ovl_ranges[id];
    memcpy(r->start, r->pristine, (size_t)(r->end - r->start));
    printf("[host] Ovl_Load(%d): %s data restored, bss zeroed\n", id, r->name);
    fflush(stdout);
}
