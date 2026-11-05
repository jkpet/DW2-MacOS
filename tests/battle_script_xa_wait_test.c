#include "common.h"
#include "stag3000/stag3000.h"
#include "stag3000/battlescript.h"

static Actor *xa_actor;

Actor *Task_GetActor(s32 ref) {
    (void)ref;
    return xa_actor;
}

void Task_SetState0(Actor *actor, u32 state) {
    actor->stateLevel0 = state;
}

void Task_SetState1(Actor *actor, u32 state) {
    actor->stateLevel1 = state;
}

int main(void) {
    Actor parent = { 0 };
    Actor xa = { 0 };
    Stg30Slots slots = { 0 };
    Stg30WorkPc work = { 0 };
    s16 script[] = { 23, 0, 0, 24 };

    parent.stateLevel1 = 1;
    slots.xaTask = 123;
    work.pc = script;
    xa_actor = NULL; /* model a child task whose handle has expired */

    if (Stg30_BattleScriptWaitForXa(&parent, &slots, &work) != 1) {
        return 1;
    }
    if (slots.xaTask != 0 || parent.stateLevel1 != 0 || work.pc != script + 3) {
        return 1;
    }

    parent.stateLevel1 = 1;
    slots.xaTask = 123;
    work.pc = script;
    xa.stateLevel0 = 0;
    xa_actor = &xa;
    if (Stg30_BattleScriptWaitForXa(&parent, &slots, &work) != 0 || work.pc != script ||
        parent.stateLevel1 != 1) {
        return 1;
    }

    xa.stateLevel0 = 1;
    if (Stg30_BattleScriptWaitForXa(&parent, &slots, &work) != 1 || xa.stateLevel0 != 2 ||
        parent.stateLevel1 != 0 || work.pc != script + 3) {
        return 1;
    }
    return 0;
}
