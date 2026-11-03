#include "common.h"
#include "stag3000/stag3000.h"
#include "stag3000/actionload.h"

#include <unistd.h>

static s16 sScript[] = { 0x18 };
Stg30Battle Stg30_Battle;

EntA0 *Cd_GetFileEntry(u32 id) {
    (void)id;
    return NULL;
}

s32 Cd_GetFileLba(s32 file) {
    (void)file;
    return 0;
}

s32 Cd_GetFileState(s32 file) {
    (void)file;
    return 0;
}

s32 Digi_GetAnimFile(s32 digiId, s32 animId) {
    (void)digiId;
    (void)animId;
    return 0;
}

s32 Digi_GetModelFile(s32 digiId) {
    (void)digiId;
    return 0;
}

s32 Skill_GetCastAnim(s32 skillId) {
    (void)skillId;
    return 0;
}

void Skill_GetFxSet(s32 skillId, s32 set, s16 *a, s16 *b) {
    (void)skillId;
    (void)set;
    (void)a;
    (void)b;
}

s32 Skill_GetPartsEntry(s32 id) {
    (void)id;
    return 0;
}

void Cd_QueueFile(s32 file) {
    (void)file;
}

void Task_NextState1(Actor *actor) {
    actor->stateLevel1++;
}

void Task_NextState0(Actor *actor) {
    actor->stateLevel0++;
}

int main(void) {
    Stg30ActionLoadWork work = { 0 };
    Actor actor = { 0 };

    if ((uptr)sScript <= UINT32_MAX) {
        return 1;
    }

    actor.work = (ActorWork *)&work;
    Stg30_ActionLoadInit(&actor, (sptr)sScript);

    if ((uptr)work.script != (uptr)sScript) {
        (void)write(1, (u8 *)"actionload_script_pointer_test: FAIL (script pointer truncated)\n",
                    sizeof("actionload_script_pointer_test: FAIL (script pointer truncated)\n") - 1);
        return 2;
    }

    Stg30_ActionLoadUpdate(&actor);
    if (actor.stateLevel1 != 1) {
        return 3;
    }

    (void)write(1, (u8 *)"actionload_script_pointer_test: PASS\n",
                sizeof("actionload_script_pointer_test: PASS\n") - 1);
    return 0;
}
