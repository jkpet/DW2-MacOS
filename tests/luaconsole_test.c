#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "common.h"
#include "host/luaconsole.h"
#include "main/156C.h"

GameState Save_GameState;

static int failures;

static void check(int ok, const char *name) {
    if (!ok) {
        printf("luaconsole_test: FAIL (%s)\n", name);
        failures++;
    }
}

int main(void) {
    char output[96 * 384];

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
