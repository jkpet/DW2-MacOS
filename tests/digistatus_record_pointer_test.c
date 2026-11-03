#include "common.h"
#include "main/cd.h"
#include "main/digilist.h"
#include "main/digistatus.h"
#include "main/game.h"
#include "main/model.h"
#include "main/task.h"
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

static MenuCtx sMenuContext;
static u32 sFileEntries[16];
static ActorModel sModel;
static sigjmp_buf sFaultJump;

static void catch_bad_record_access(int signalNumber) {
    (void)signalNumber;
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

void Snd_PlayById(s32 id, s32 set) {
    (void)id;
    (void)set;
}

void Actor_InitTransform(ContC40 *actor, s32 *pos, u16 rotY) {
    (void)actor;
    (void)pos;
    (void)rotY;
}

s32 Digi_GetModelFile(s32 id) { return id; }
s32 Digi_GetAnimFile(s32 id, s32 anim) { return id + anim; }

ActorModel *Gfx_AttachModel(Actor *actor, s32 file) {
    (void)actor;
    (void)file;
    return &sModel;
}

void Cd_QueueFile(s32 file) { (void)file; }
void GsInitCoordinate2(GsCOORDINATE2 *parent, GsCOORDINATE2 *coord) {
    (void)parent;
    (void)coord;
}

EntA0 *Cd_GetFileEntry(u32 id) {
    (void)id;
    return (EntA0 *)sFileEntries;
}

s32 GsSetFlatLight(s32 index, GsF_LIGHT *light) {
    (void)index;
    (void)light;
    return 0;
}

void GsSetAmbient(s32 r, s32 g, s32 b) {
    (void)r;
    (void)g;
    (void)b;
}

void GsSetLightMode(s32 mode) { (void)mode; }

static int fail(const char *message) {
    puts(message);
    return 1;
}

int main(void) {
    u8 *region = mmap((void *)(uptr)0x110000000, TEST_REGION_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANON, -1, 0);
    u8 *record;
    MenuDigiPickWork pickWork = { 0 };
    MenuDigiPickWork multiPickWork = { 0 };
    MenuDigiStatusInitWork statusWork = { 0 };
    Actor pickActor = { 0 };
    Actor multiPickActor = { 0 };
    Actor statusActor = { 0 };
    if (region == MAP_FAILED) {
        return fail("digistatus_record_pointer_test: could not allocate roster record");
    }
    if ((uptr)region <= UINT32_MAX) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digistatus_record_pointer_test: record is not above 32-bit address range");
    }

    Ps1_Ram = region;
    record = region + RECORD_OFFSET;
    record[0] = 1;
    record[1] = 0x2A;
    Menu_Ctx = &sMenuContext;
    pickWork.rows[0].kind = 1;
    pickWork.rows[0].record = (s32)P32_SET(record);
    Pad_State[0].cross = 1;
    pickActor.work = (ActorWork *)&pickWork;
    Menu_ConfirmSinglePick(&pickActor);

    statusActor.work = (ActorWork *)&statusWork;
    signal(SIGSEGV, catch_bad_record_access);
    signal(SIGBUS, catch_bad_record_access);
    if (sigsetjmp(sFaultJump, 1) != 0) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digistatus_record_pointer_test: status initializer faulted reading selected record");
    }
    Menu_DigiStatusInit(&statusActor, 1);
    if (statusActor.digiId != record[1]) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digistatus_record_pointer_test: wrong digi id read from selected record");
    }

    multiPickWork.pickMax = 1;
    multiPickWork.rows[0].pickState = 2;
    multiPickWork.rows[0].record = (s32)P32_SET(record);
    multiPickActor.work = (ActorWork *)&multiPickWork;
    Pad_State[0].cross = 1;
    Menu_ConfirmMultiPick(&multiPickActor);
    if (Menu_Ctx->pickedRecords[0] != record) {
        munmap(region, TEST_REGION_SIZE);
        return fail("digistatus_record_pointer_test: multi-pick did not restore the selected record");
    }

    munmap(region, TEST_REGION_SIZE);
    puts("digistatus_record_pointer_test: PASS");
    return 0;
}
