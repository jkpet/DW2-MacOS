#include "common.h"
#include "main/digidata.h"
#include "stag3000/stag3000.h"

extern int puts(const char *);
void Stg30_ResultUpdate(Actor *a0);
void Stg30_BattleWonUpdate(Actor *a0);

Stg30Battle Stg30_Battle;
GameState Save_GameState;
PadState Pad_State[2];
DungState Dung_State;
SysState Sys_State;

static int multiplier_percent = 200;
static int stop_at_digivolution_levels = 1;
static Stg30SkillLearnArgs captured_skill_args;

int Host_ExpMultiplierPercent(void) {
    return multiplier_percent;
}

int Host_StopAtDigivolutionLevels(void) {
    return stop_at_digivolution_levels;
}

void Mem_FillWordsNeg1(s32 *words, s32 count) {
    while (count-- > 0) {
        *words++ = -1;
    }
}

void Task_NextState0(Actor *actor) {
    actor->stateLevel0++;
}

void Task_NextState1(Actor *actor) {
    actor->stateLevel1++;
}

void Task_SetState0(Actor *actor, u32 state) {
    actor->stateLevel0 = state;
}

void Task_NextState2(Actor *actor) {
    actor->stateLevel2++;
    actor->stateLevel3 = 0;
    actor->stateLevel4 = 0;
}

void Task_NextState3(Actor *actor) {
    actor->stateLevel3++;
    actor->stateLevel4 = 0;
}

void Task_SetState4(Actor *actor, u32 state) {
    actor->stateLevel4 = state;
}

void Task_NextState4(Actor *actor) {
    actor->stateLevel4++;
}

void Task_SetState01(Actor *actor, u32 state0, u32 state1) {
    actor->stateLevel0 = state0;
    actor->stateLevel1 = state1;
}

void Task_SetState2(Actor *actor, u32 state) {
    actor->stateLevel2 = state;
}

void Task_Create(u32 id, s32 *slot, sptr arg) {
    if (id == 0x512) {
        captured_skill_args = *(Stg30SkillLearnArgs *)arg;
        *slot = 1;
    }
}

Actor *Task_GetActor(s32 ref) {
    (void)ref;
    return NULL;
}

void Mem_Zero(void *memory, s32 bytes) {
    u8 *p = memory;
    while (bytes-- > 0) {
        *p++ = 0;
    }
}

s32 Digi_GetRank(s32 id) {
    (void)id;
    return 0;
}

s32 Digi_GetStatGrowth(s32 id, s32 stat) {
    (void)id;
    (void)stat;
    return 0;
}

s32 Rand_Next(void) {
    return 0;
}

s32 Cd_GetFileState(s32 id) {
    (void)id;
    return 3;
}

void Cd_QueueFile(s32 id) {
    (void)id;
}

void Cd_QueueStag4000Files(void) {}

void Snd_PlayById(s32 id, s32 set) {
    (void)id;
    (void)set;
}

void Stg30_SetCameraShot(u8 state) {
    (void)state;
}

void Stg30_FighterSetVisible(Actor *actor, s32 visible) {
    (void)actor;
    (void)visible;
}

void Stg30_FighterQueueHomeReset(Actor *actor) {
    (void)actor;
}

s32 Skill_GetRank(s32 id) {
    (void)id;
    return 0;
}

s32 Flag_Test(s32 id) {
    (void)id;
    return 0;
}

void Flag_Set(s32 id, s32 value) {
    (void)id;
    (void)value;
}

void Gfx_FadeOutToBlack(s32 frames) {
    (void)frames;
}

EntA0 *Cd_GetFileEntry(u32 id) {
    (void)id;
    return NULL;
}

void Text_Open(void *text, TextOpenArgs *args) {
    (void)text;
    (void)args;
}

static int fail(const char *message) {
    puts(message);
    return 1;
}

static s32 skill_count(const Stg30SkillLearnArgs *args) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < 12; i++) {
        count += args->skillIds[i] != 0;
    }
    return count;
}

int main(void) {
    Stg30ResultWork work = { 0 };
    Stg30BattleChildren children = { 0 };
    Actor result = { 0 };
    Actor battle = { 0 };
    DigiRosterEntry *knockedOut = &Stg30_Battle.digis[0];
    DigiRosterEntry *standing = &Stg30_Battle.digis[1];

    work.pair.field_0 = 57;
    result.work = (ActorWork *)&work;
    knockedOut->digiId = 1;
    knockedOut->level = 1;
    knockedOut->maxLevel = 5;
    knockedOut->maxHp = 100;
    knockedOut->maxMp = 50;
    knockedOut->hp = 0;
    standing->digiId = 2;
    standing->level = 1;
    standing->maxLevel = 5;
    standing->maxHp = 100;
    standing->maxMp = 50;
    standing->hp = 40;

    Stg30_ResultUpdate(&result);

    if (work.pair.field_0 != 114) {
        return fail("battle result did not display the scaled EXP reward");
    }
    if (knockedOut->exp != 114 || knockedOut->level != 5 || knockedOut->hp != 0) {
        return fail("defeated party member did not gain all eligible levels while remaining knocked out");
    }
    if (standing->exp != 114 || standing->level != 5 || standing->hp != standing->maxHp) {
        return fail("standing party member did not receive the scaled EXP and all level-ups");
    }
    if (Stg30_Battle.digis[2].exp != 0 || Stg30_Battle.leveledUp[2] != 0) {
        return fail("empty party slot received EXP");
    }
    if (Stg30_Battle.leveledUp[0] != 4 || Stg30_Battle.leveledUp[1] != 4) {
        return fail("battle result did not queue each crossed level event");
    }

    battle.u34.children = (sptr)&children;
    battle.stateLevel2 = 3;
    battle.stateLevel3 = 1;
    static const s32 expected_skill_counts[] = { 3, 2, 2, 1 };
    for (s32 skill_event = 0; skill_event < 4; skill_event++) {
        knockedOut->learnableSkills[skill_event] = skill_event + 1;
        knockedOut->learnableSkills[skill_event + 4] = skill_event + 5;
    }
    for (s32 event = 0; event < 4; event++) {
        Stg30_BattleWonUpdate(&battle);
        if (children.resultTask == 0 || captured_skill_args.slot != 0 ||
            skill_count(&captured_skill_args) != expected_skill_counts[event]) {
            return fail("battle skill progression did not process skills for each earned level");
        }
        children.resultTask = 0;
        Stg30_BattleWonUpdate(&battle);
        if (Stg30_Battle.leveledUp[0] != 3 - event) {
            return fail("battle skill progression did not consume one level event at a time");
        }
        if (event < 3 && battle.stateLevel3 != 1) {
            return fail("battle advanced past a Digimon with pending level events");
        }
    }
    if (battle.stateLevel3 != 2) {
        return fail("battle did not advance after consuming the final level event");
    }
    for (s32 event = 0; event < 4; event++) {
        Stg30_BattleWonUpdate(&battle);
        if (Stg30_Battle.leveledUp[1] != 3 - event) {
            return fail("battle skipped a level event for a Digimon with no new skills");
        }
    }
    if (battle.stateLevel3 != 3) {
        return fail("battle did not advance after all party level events were consumed");
    }

    standing->level = 9;
    standing->maxLevel = 40;
    standing->exp = DIGI_EXP_MAX - 114;
    result.stateLevel0 = 0;
    Stg30_ResultUpdate(&result);
    if (standing->exp != DIGI_EXP_MAX || standing->level != 11 || Stg30_Battle.leveledUp[1] != 2) {
        return fail("battle level-ups did not stop at level 11 while retaining excess EXP");
    }

    result.stateLevel0 = 0;
    Stg30_ResultUpdate(&result);
    if (standing->level != 21 || Stg30_Battle.leveledUp[1] != 10) {
        return fail("a later EXP award did not resume progression to the next milestone");
    }

    stop_at_digivolution_levels = 0;
    standing->level = 9;
    standing->exp = DIGI_EXP_MAX - 114;
    result.stateLevel0 = 0;
    Stg30_ResultUpdate(&result);
    if (standing->level != 40 || Stg30_Battle.leveledUp[1] != 31) {
        return fail("disabled milestone stops did not restore the original multi-level behavior");
    }

    puts("battle_exp_test: all passed");
    return 0;
}
