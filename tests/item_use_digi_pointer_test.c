#include "common.h"
#include "main/digilist.h"
#include "main/game.h"
#include "main/itemeffect.h"
#include "main/topmenu.h"

#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>

extern int puts(const char *);

#define TEST_REGION_SIZE 0x1000
#define RECORD_OFFSET 0x200

u_char *Ps1_Ram;
MenuCtx *Menu_Ctx;
PadState Pad_State[2];
GameState *Save_GameStatePtr;
DungState *Dung_StatePtr;

static MenuCtx sMenuContext;
static ItemEffect sHealthItem = { 1, 0, 25 };
static ItemStatEffect sStatBoosts[] = { { 4, 6, 3 }, { 4, 10, 25 } };
static sigjmp_buf sFaultJump;
static int sExpMultiplierPercent = 500;

int Host_ExpMultiplierPercent(void) {
    return sExpMultiplierPercent;
}

static void catch_bad_record_access(int signalNumber) {
    (void)signalNumber;
    siglongjmp(sFaultJump, 1);
}

s32 Menu_GridIndexColMajor(s16 *cursor, s16 *grid) {
    (void)cursor;
    (void)grid;
    return 0;
}

u8 Digi_GetType(s32 digiId) {
    return (u8)digiId;
}

s32 Item_GetNameText(s32 itemId) {
    return itemId;
}

void Text_OpenDesc(void *text, TextDesc *desc) {
    (void)text;
    (void)desc;
}

EntA0 *Cd_GetFileEntry(u32 id) {
    if (id == 0x5130000) {
        return (EntA0 *)&sHealthItem;
    }
    if (id == 0x5130001) {
        return (EntA0 *)sStatBoosts;
    }
    return NULL;
}

void Snd_PlayById(s32 id, s32 set) {
    (void)id;
    (void)set;
}

void Task_SetState1(Actor *actor, u32 state) {
    (void)actor;
    (void)state;
}

void Item_RemoveFromBag(s32 slot) {
    (void)slot;
}

s32 Beetle_GetPart(s32 slot) {
    (void)slot;
    return 0;
}

void Beetle_SetPartBroken(s32 slot, s32 broken) {
    (void)slot;
    (void)broken;
}

s32 Rand_Next(void) {
    return 0;
}

static int fail(const char *message) {
    puts(message);
    return 1;
}

int main(void) {
    u8 *region = mmap((void *)(uptr)0x110000000, TEST_REGION_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    DigiRosterEntry *record;
    MenuDigiPickWork pickWork = { 0 };
    Actor pickActor = { 0 };

    if (region == MAP_FAILED) {
        return fail("could not allocate roster record");
    }
    if ((uptr)region <= UINT32_MAX) {
        munmap(region, TEST_REGION_SIZE);
        return fail("roster record is not above the 32-bit address range");
    }

    Ps1_Ram = region;
    Menu_Ctx = &sMenuContext;
    Menu_Ctx->itemId = 0x78;
    record = (DigiRosterEntry *)(region + RECORD_OFFSET);
    record->digiId = 2;
    record->maxHp = 100;
    record->hp = 40;
    record->attack = 20;
    record->exp = 100;
    pickWork.rows[0].kind = 1;
    pickWork.rows[0].record = (s32)P32_SET(record);
    pickActor.work = (ActorWork *)&pickWork;
    Pad_State[0].cross = 1;

    signal(SIGSEGV, catch_bad_record_access);
    signal(SIGBUS, catch_bad_record_access);
    if (sigsetjmp(sFaultJump, 1) != 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("menu health-item use faulted reading the selected roster record");
    }

    Menu_UseItemOnDigi(&pickActor);

    if (record->hp != 65) {
        munmap(region, TEST_REGION_SIZE);
        return fail("health item did not update the selected Digimon's HP");
    }

    Menu_Ctx->itemId = 0xD0;
    if (sigsetjmp(sFaultJump, 1) != 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("menu stat-boost item faulted reading the selected roster record");
    }
    Menu_UseItemOnDigi(&pickActor);
    if (record->attack != 21) {
        munmap(region, TEST_REGION_SIZE);
        return fail("stat-boost item did not update the selected Digimon's attack");
    }

    Menu_Ctx->itemId = 0xD1;
    Menu_UseItemOnDigi(&pickActor);
    if (record->exp != 225) {
        munmap(region, TEST_REGION_SIZE);
        return fail("EXP item did not apply the selected multiplier");
    }
    sExpMultiplierPercent = 2000;
    record->exp = 99999990;
    Menu_UseItemOnDigi(&pickActor);
    if (record->exp != 99999999) {
        munmap(region, TEST_REGION_SIZE);
        return fail("EXP item did not cap the resulting total");
    }

    munmap(region, TEST_REGION_SIZE);
    puts("item_use_digi_pointer_test: PASS");
    return 0;
}
