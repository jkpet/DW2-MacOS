#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "common.h"
#include "host/luaconsole.h"
#include "main/156C.h"
#include "main/itemextension.h"

GameState Save_GameState;

ItemTableEntry *Item_FindById(s32 id) {
    static ItemTableEntry occupied;
    return id > 0 && id <= 0xFF ? &occupied : NULL;
}

s32 Item_AddToBag(s32 id) {
    s32 i;
    for (i = 0; i < 0x30; i++) {
        if (Save_GameState.bagItems[i] == 0) {
            Save_GameState.bagItems[i] = (u16)id;
            return i;
        }
    }
    return -1;
}

void Item_SortList(void) {
}

u8 *Digi_GetDefaultName(s32 id) {
    static u8 agumon[] = { 0x0A, 0x2A, 0x38, 0x30, 0x32, 0x31, 0xFF };
    static u8 gabumon[] = { 0x10, 0x24, 0x25, 0x38, 0x30, 0x32, 0x31, 0xFF };

    return id == 42 ? agumon : gabumon;
}

static int failures;

static void check(int ok, const char *name) {
    if (!ok) {
        printf("luaconsole_test: FAIL (%s)\n", name);
        failures++;
    }
}

int main(void) {
    char output[96 * 384];
    char completions[256];
    static const u8 agumon_name[14] = { 0x0A, 0x2A, 0x38, 0x30, 0x32, 0x31, 0xFF };
    static const u8 gabumon_name[14] = { 0x10, 0x24, 0x25, 0x38, 0x30, 0x32, 0x31, 0xFF };

    if (!SDL_Init(0)) {
        printf("luaconsole_test: FAIL (SDL_Init: %s)\n", SDL_GetError());
        return 1;
    }

    LuaConsole_SetEnabled(0);
    check(LuaConsole_Init(), "disabled init");
    check(!LuaConsole_Enabled(), "disabled console stays unavailable");
    LuaConsole_Shutdown();

    LuaConsole_SetEnabled(1);
    check(LuaConsole_Init(), "enabled init");
    check(LuaConsole_Enabled(), "console enabled");
    check(LuaConsole_Complete("giveI", completions, sizeof(completions)) == 1 &&
              strcmp(completions, "giveItem") == 0,
          "completion finds registered console functions");
    check(LuaConsole_Complete("whi", completions, sizeof(completions)) == 1 &&
              strcmp(completions, "while") == 0,
          "completion includes Lua keywords");
    check(LuaConsole_Complete("add", completions, sizeof(completions)) == 3 && strstr(completions, "addBits") != NULL &&
              strstr(completions, "addExp") != NULL && strstr(completions, "addMaxLevel") != NULL,
          "completion returns all matching globals");

    Save_GameState.bits = 100;
    LuaConsole_Submit("addBits(500)");
    check(Save_GameState.bits == 100, "game state is not changed from the Lua/UI thread");
    LuaConsole_Apply();
    check(Save_GameState.bits == 600, "positive adjustment applied on game thread");

    LuaConsole_Submit("addBits(-1000)");
    LuaConsole_Apply();
    check(Save_GameState.bits == 0, "negative adjustment clamps at zero");

    Save_GameState.bits = 99999900;
    LuaConsole_Submit("addBits(1000)");
    LuaConsole_Apply();
    check(Save_GameState.bits == 99999999, "positive adjustment clamps at maximum");

    LuaConsole_Submit("addBits(1.5)");
    LuaConsole_Apply();
    check(Save_GameState.bits == 99999999, "fractional amount rejected");

    memset(&Save_GameState.elems[0], 0, sizeof(Save_GameState.elems[0]));
    memset(&Save_GameState.elems[1], 0, sizeof(Save_GameState.elems[1]));
    Save_GameState.elems[0].state = 3;
    Save_GameState.elems[0].digiId = 42;
    Save_GameState.elems[0].dp = 10;
    memcpy(Save_GameState.elems[0].name, agumon_name, sizeof(agumon_name));
    Save_GameState.elems[1].state = 4;
    Save_GameState.elems[1].digiId = 17;
    Save_GameState.elems[1].dp = 99;
    memcpy(Save_GameState.elems[1].name, gabumon_name, sizeof(gabumon_name));

    LuaConsole_Submit("printPartyList()");
    LuaConsole_Apply();
    LuaConsole_CopyOutput(output, sizeof(output));
    check(strstr(output, "Agumon - 42") != NULL, "party list prints decoded name and decimal id");
    check(strstr(output, "Gabumon - 17") != NULL, "party list includes all party slots");

    LuaConsole_Submit("changeDP(42, -4)");
    check(Save_GameState.elems[0].dp == 10, "DP is not changed from the Lua/UI thread");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].dp == 6, "negative DP adjustment applied");

    LuaConsole_Submit("changeDP(42, 1000)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].dp == 99, "DP adjustment clamps at 99");

    LuaConsole_Submit("changeDP(42, -1000)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].dp == 0, "DP adjustment clamps at zero");

    Save_GameState.elems[2].state = 5;
    Save_GameState.elems[2].digiId = 42;
    Save_GameState.elems[2].dp = 50;
    LuaConsole_Submit("changeDP(42, 1)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].dp == 0 && Save_GameState.elems[2].dp == 50,
          "duplicate party ids are not changed ambiguously");
    LuaConsole_CopyOutput(output, sizeof(output));
    check(strstr(output, "appears more than once") != NULL, "duplicate party id is reported");
    memset(&Save_GameState.elems[2], 0, sizeof(Save_GameState.elems[2]));

    LuaConsole_Submit("changeDP(43, 1)");
    LuaConsole_Apply();
    LuaConsole_CopyOutput(output, sizeof(output));
    check(strstr(output, "Digimon id 43 is not in the party") != NULL, "unknown party id is reported");

    LuaConsole_Submit("changeDP(999, 1)");
    LuaConsole_CopyOutput(output, sizeof(output));
    check(strstr(output, "id must be in the range 0..255") != NULL, "out-of-range DP id is rejected");

    Save_GameState.elems[0].level = 10;
    Save_GameState.elems[0].maxLevel = 20;
    Save_GameState.elems[0].exp = 500;
    LuaConsole_Submit("addExp(42, 100)");
    check(Save_GameState.elems[0].exp == 500, "EXP is not changed from the Lua/UI thread");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].exp == 600, "EXP amount is applied");

    LuaConsole_Submit("addExp(42, -1000)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].exp == 0, "EXP adjustment clamps at zero");

    LuaConsole_Submit("addExp(42, 999999999)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].exp == 99999999, "EXP adjustment clamps at maximum");

    LuaConsole_Submit("addExp(42, 1.5)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].exp == 99999999, "fractional EXP amount is rejected");

    LuaConsole_Submit("addMaxLevel(42, 5)");
    check(Save_GameState.elems[0].maxLevel == 20, "max level is not changed from the Lua/UI thread");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].maxLevel == 25, "max-level adjustment is applied");

    LuaConsole_Submit("addMaxLevel(42, -100)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].maxLevel == 10, "max level cannot be reduced below current level");

    LuaConsole_Submit("addMaxLevel(42, 1000)");
    LuaConsole_Apply();
    check(Save_GameState.elems[0].maxLevel == 99, "max level clamps at 99");

    LuaConsole_Submit("giveItem('InfMissile', 2)");
    check(Save_GameState.bagItems[0] == 0, "source item grant is queued from the Lua/UI thread");
    LuaConsole_Apply();
    {
        const ItemExtension *item = ItemExtension_FindByKey(0x494E464Du);
        LuaConsole_CopyOutput(output, sizeof(output));
        check(strstr(output, "giveItem: source item is unavailable") == NULL,
              "source item gets a runtime ID when all byte-sized IDs are occupied");
        check(item != NULL && Save_GameState.bagItems[0] == item->runtimeId &&
                  Save_GameState.bagItems[1] == item->runtimeId,
              "source item grant is applied on the game thread");
    }
    LuaConsole_Submit("giveItem('UnknownItem', 1)");
    LuaConsole_CopyOutput(output, sizeof(output));
    check(strstr(output, "unknown source item") != NULL, "unknown source item grant is rejected");

    LuaConsole_Submit("while true do end");
    LuaConsole_CopyOutput(output, sizeof(output));
    check(strstr(output, "instruction budget exceeded") != NULL, "infinite loop is interrupted");

    LuaConsole_Shutdown();
    SDL_Quit();
    if (failures == 0) {
        printf("luaconsole_test: PASS\n");
    }
    return failures != 0;
}
