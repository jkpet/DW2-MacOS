#include "psyq_types.h"
#include "ps1mem.h"

#if defined(DW2_NATIVE) && defined(__APPLE__)
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

/* Reserve PS1 RAM and the global ordering tables in one 16 MB GPU-link window. macOS ASLR slides
 * the PIE image at 64 KB granularity, so a static 2 MB Ps1_Ram can straddle a 24-bit link window.
 * Keep this arena in the executable's 4 GB region too: P32 reconstructs legacy pointer words from
 * the high bits shared by the image and Ps1_Ram. Use non-fixed mmap hints, accepting only an exact
 * free aligned address so no existing mapping can be replaced. */
#define PS1_LINK_WINDOW_SIZE 0x01000000u
#define PS1_LINK_WINDOW_START ((uintptr_t)0x110000000ULL)
#define PS1_LINK_REGION_END ((uintptr_t)0x200000000ULL)

u_char *Ps1_Ram;

int Ps1Mem_Init(void) {
    uintptr_t address;

    if (Ps1_Ram != 0) {
        return 0;
    }
    for (address = PS1_LINK_WINDOW_START; address + PS1_LINK_WINDOW_SIZE <= PS1_LINK_REGION_END;
         address += PS1_LINK_WINDOW_SIZE) {
        void *arena = mmap((void *)address, PS1_LINK_WINDOW_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANON, -1, 0);

        if (arena == MAP_FAILED) {
            continue;
        }
        if ((uintptr_t)arena != address) {
            munmap(arena, PS1_LINK_WINDOW_SIZE);
            continue;
        }
        Ps1_Ram = arena;
        return 0;
    }
    return ENOMEM;
}
#else
/* PS1 main RAM (2 MB) and the scratchpad. The game's heap is Ps1_Ram + 0x75000 .. + 0x1FF000
 * (Mem_HeapStart, Sys_Main); the low part stays unused (main exe and overlay area on the PS1). */
u_char Ps1_Ram[PS1_RAM_SIZE] __attribute__((aligned(16)));
#endif

u_char Ps1_Scratchpad[PS1_SCRATCHPAD_SIZE] __attribute__((aligned(16)));
