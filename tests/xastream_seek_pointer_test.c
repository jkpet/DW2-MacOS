#include "common.h"
#include "stag2000/stag2000.h"

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static const s32 sExpectedLba = 148789;

static void fail_on_fault(int signal_number) {
    static const char message[] = "xastream_seek_pointer_test: FAIL (CD seek dereferenced an invalid location pointer)\n";

    (void)signal_number;
    (void)write(1, message, sizeof(message) - 1);
    _exit(10);
}

void Stg20_XaStreamUpdate(Actor *actor);

s32 Cd_GetFileLba(s32 file) {
    (void)file;
    return sExpectedLba;
}

void Task_NextState1(Actor *actor) {
    actor->stateLevel1++;
}

void Task_NextState0(Actor *actor) {
    actor->stateLevel0++;
}

void Task_SetState0(Actor *actor, u32 state) {
    actor->stateLevel0 = state;
}

void Task_DefaultDestroy(Actor *actor) {
    (void)actor;
}

int Host_PakFileAt(int lba, int *sectors) {
    (void)lba;
    *sectors = 0;
    return -1;
}

int Host_PakSector(int lba, unsigned char *body) {
    (void)lba;
    (void)body;
    return -1;
}

unsigned long long Host_VBlankCount(void) {
    return 0;
}

void XaDec_Reset(void) {}
void XaDec_Sector(const uint8_t *body) { (void)body; }
void XaDec_Stop(void) {}
int XaDec_Underruns(void) { return 0; }

void Psyq_Log(int *seen, const char *func, const char *fmt, ...) {
    (void)seen;
    (void)func;
    (void)fmt;
}

int main(void) {
    Stg20XaWork work = { 0 };
    Actor actor = { 0 };
    u8 result[8];

    signal(SIGSEGV, fail_on_fault);
    signal(SIGBUS, fail_on_fault);

    work.fileId = 0xDDB;
    work.len = 8;
    work.channel = 1;
    actor.work = (ActorWork *)&work;

    Stg20_XaStreamUpdate(&actor);
    if (actor.stateLevel1 != 1) {
        return 1;
    }

    CdControlB(CdlGetlocP, NULL, result);
    s32 actualLba = CdPosToInt((CdlLOC *)(void *)(result + 5));
    if (actualLba != sExpectedLba) {
        char message[128];
        int length = snprintf(message, sizeof(message),
                              "xastream_seek_pointer_test: FAIL (expected LBA %d, got %d)\n", sExpectedLba,
                              actualLba);
        (void)write(1, message, (size_t)length);
        return 2;
    }

    (void)write(1, (u8 *)"xastream_seek_pointer_test: PASS\n",
                sizeof("xastream_seek_pointer_test: PASS\n") - 1);
    return 0;
}
