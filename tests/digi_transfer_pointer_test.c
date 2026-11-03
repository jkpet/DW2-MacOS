#include "common.h"
#include "main/digilist.h"
#include "main/game.h"
#include "main/topmenu.h"
#include "main/task.h"

#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>

extern int puts(const char *);

#define TEST_REGION_SIZE 0x1000
#define RECORD_OFFSET 0x250

u_char *Ps1_Ram;
MenuCtx *Menu_Ctx;
PadState Pad_State[2];

static MenuCtx sMenuContext;
static sigjmp_buf sFaultJump;
static u32 sFileEntries[16];
static volatile sig_atomic_t sBadAccess;
static volatile sig_atomic_t sTextOpened;

typedef struct {
    MenuDigiPickWork work;
    s16 extraPickIndices[3];
} MenuDigiTransferTestWork;

static void catch_bad_transfer_record_access(int signalNumber) {
    (void)signalNumber;
    sBadAccess = 1;
    siglongjmp(sFaultJump, 1);
}

s32 Menu_GridIndexColMajor(s16 *cursor, s16 *grid) {
    (void)cursor;
    (void)grid;
    return 0;
}

void Task_SetState1(Actor *actor, u32 state) {
    (void)actor;
    (void)state;
}

void Text_Close(s32 *slot) { (void)slot; }

void Text_OpenPacked(void *text, s32 file, u32 mode, Halves position) {
    (void)text;
    (void)file;
    (void)mode;
    (void)position;
    sTextOpened = 1;
    siglongjmp(sFaultJump, 1);
}

void Text_OpenDesc(void *text, TextDesc *desc) {
    (void)text;
    (void)desc;
}

EntA0 *Cd_GetFileEntry() {
    return (EntA0 *)sFileEntries;
}

u8 *Digi_GetDefaultName(s32 id) {
    (void)id;
    return (u8 *)sFileEntries;
}

void Snd_PlayById(s32 id, s32 set) {
    (void)id;
    (void)set;
}

static int fail(const char *message) {
    puts(message);
    return 1;
}

int main(void) {
    u8 *region = mmap((void *)(uptr)0x110000000, TEST_REGION_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    DigiRosterSwapRec *record;
    MenuDigiTransferTestWork sourceWork = { 0 };
    MenuDigiTransferTestWork placeWork = { 0 };
    Actor sourceActor = { 0 };
    Actor placeActor = { 0 };

    if (region == MAP_FAILED) {
        return fail("digi_transfer_pointer_test: could not allocate transfer record");
    }
    if ((uptr)region <= UINT32_MAX) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: record is not above 32-bit address range");
    }

    Ps1_Ram = region;
    Menu_Ctx = &sMenuContext;
    record = (DigiRosterSwapRec *)(region + RECORD_OFFSET);
    record->state = 1;

    sourceWork.work.rows[0].kind = 1;
    sourceWork.work.rows[0].record = (s32)P32_SET(record);
    sourceActor.work = (ActorWork *)&sourceWork.work;
    placeActor.work = (ActorWork *)&placeWork.work;
    Pad_State[0].cross = 1;

    Menu_DigiTransferPickSrc(&sourceActor);
    if ((u32)Menu_Ctx->transferRecord != P32_SET(record)) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: source selection changed the stored record word");
    }

    signal(SIGSEGV, catch_bad_transfer_record_access);
    signal(SIGBUS, catch_bad_transfer_record_access);
    if (sigsetjmp(sFaultJump, 1) == 0) {
        Menu_DigiTransferPlace(&placeActor);
    }
    if (sBadAccess != 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: placement faulted reading the selected transfer record");
    }
    if (sTextOpened == 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: placement did not reach the transfer confirmation message");
    }

    if (record->state != 2) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: placement did not update the selected record");
    }
    if ((u32)placeWork.work.rows[0].record != P32_SET(record)) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: destination row did not retain the selected record");
    }

    sBadAccess = 0;
    sTextOpened = 0;
    if (sigsetjmp(sFaultJump, 1) == 0) {
        Menu_DigiTransferPlace(&placeActor);
    }
    if (sBadAccess != 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: swap faulted reading a destination row record");
    }
    if (sTextOpened == 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: swap did not reach its confirmation message");
    }
    if (record->state != 2) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digi_transfer_pointer_test: swap did not preserve the transfer record state");
    }

    munmap(region, TEST_REGION_SIZE);
    puts("digi_transfer_pointer_test: PASS");
    return 0;
}
