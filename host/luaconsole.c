#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "common.h"
#include "host/luaconsole.h"
#include "main/156C.h"
#include "main/itemextension.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define LUA_CONSOLE_QUEUE 64
#define LUA_CONSOLE_OUTPUT_LINES 96
#define LUA_CONSOLE_OUTPUT_LINE 384
#define LUA_CONSOLE_INPUT_MAX 4096
#define LUA_CONSOLE_MEMORY_MAX (16u * 1024u * 1024u)
#define LUA_CONSOLE_INSTRUCTION_BUDGET 1000000
#define LUA_CONSOLE_HOOK_GRANULARITY 1000
#define GAME_BITS_MAX 99999999
#define GAME_EXP_MAX 99999999
#define GAME_MAX_LEVEL 99

extern GameState Save_GameState;
extern u8 *Digi_GetDefaultName(s32 id);
extern s32 Item_AddToBag(s32 id);
extern void Item_SortList(void);

typedef struct {
    int kind;
    int digi_id;
    uint32_t item_key;
    int64_t amount;
} GameRequest;

enum {
    GAME_REQUEST_ADD_BITS,
    GAME_REQUEST_PRINT_PARTY,
    GAME_REQUEST_CHANGE_DP,
    GAME_REQUEST_ADD_EXP,
    GAME_REQUEST_ADD_MAX_LEVEL,
    GAME_REQUEST_GIVE_ITEM
};

static int enabled;
static int is_open;
static lua_State *lua;
static SDL_Mutex *lock;
static GameRequest queue[LUA_CONSOLE_QUEUE];
static int queue_head, queue_count;
static char output[LUA_CONSOLE_OUTPUT_LINES][LUA_CONSOLE_OUTPUT_LINE];
static int output_start, output_count;
static uint64_t output_version;
static size_t lua_memory;
static int instructions_left;

static void output_line(const char *text, size_t length) {
    int at;

    if (lock == NULL) {
        return;
    }
    SDL_LockMutex(lock);
    if (output_count == LUA_CONSOLE_OUTPUT_LINES) {
        output_start = (output_start + 1) % LUA_CONSOLE_OUTPUT_LINES;
        output_count--;
    }
    at = (output_start + output_count) % LUA_CONSOLE_OUTPUT_LINES;
    if (length >= LUA_CONSOLE_OUTPUT_LINE) {
        length = LUA_CONSOLE_OUTPUT_LINE - 1;
    }
    memcpy(output[at], text, length);
    output[at][length] = 0;
    output_count++;
    output_version++;
    SDL_UnlockMutex(lock);
}

static void output_text(const char *text) {
    const char *line = text != NULL ? text : "(null)";
    const char *end;

    do {
        end = strchr(line, '\n');
        output_line(line, end != NULL ? (size_t)(end - line) : strlen(line));
        if (end == NULL) {
            break;
        }
        line = end + 1;
    } while (*line != 0);
}

uint64_t LuaConsole_CopyOutput(char *out, size_t capacity) {
    size_t used = 0;
    int i;
    uint64_t version;

    if (out == NULL || capacity == 0) {
        return 0;
    }
    out[0] = 0;
    if (lock == NULL) {
        return 0;
    }
    SDL_LockMutex(lock);
    for (i = 0; i < output_count && used + 1 < capacity; i++) {
        int at = (output_start + i) % LUA_CONSOLE_OUTPUT_LINES;
        size_t length = strlen(output[at]);
        size_t available = capacity - used - 1;

        if (length > available) {
            length = available;
        }
        memcpy(out + used, output[at], length);
        used += length;
        if (used + 1 < capacity) {
            out[used++] = '\n';
        }
    }
    out[used] = 0;
    version = output_version;
    SDL_UnlockMutex(lock);
    return version;
}

static void *lua_allocator(void *ud, void *ptr, size_t old_size, size_t new_size) {
    size_t old = ptr != NULL ? old_size : 0;
    void *res;

    (void)ud;
    if (new_size == 0) {
        free(ptr);
        lua_memory = old > lua_memory ? 0 : lua_memory - old;
        return NULL;
    }
    if (lua_memory > LUA_CONSOLE_MEMORY_MAX ||
        (new_size > old && new_size - old > LUA_CONSOLE_MEMORY_MAX - lua_memory)) {
        return NULL;
    }
    res = realloc(ptr, new_size);
    if (res != NULL) {
        lua_memory = lua_memory - old + new_size;
    }
    return res;
}

static void format_value(lua_State *state, int index, char *out, size_t capacity) {
    int type = lua_type(state, index);

    switch (type) {
    case LUA_TNIL:
        SDL_strlcpy(out, "nil", capacity);
        break;
    case LUA_TBOOLEAN:
        SDL_strlcpy(out, lua_toboolean(state, index) ? "true" : "false", capacity);
        break;
    case LUA_TNUMBER:
        if (lua_isinteger(state, index)) {
            SDL_snprintf(out, capacity, "%lld", (long long)lua_tointeger(state, index));
        } else {
            SDL_snprintf(out, capacity, "%.14g", (double)lua_tonumber(state, index));
        }
        break;
    case LUA_TSTRING: {
        size_t length;
        const char *s = lua_tolstring(state, index, &length);

        if (length >= capacity) {
            length = capacity - 1;
        }
        memcpy(out, s, length);
        out[length] = 0;
        break;
    }
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TTHREAD:
    case LUA_TUSERDATA:
    case LUA_TLIGHTUSERDATA:
        SDL_snprintf(out, capacity, "%s: %p", lua_typename(state, type), lua_topointer(state, index));
        break;
    default:
        SDL_strlcpy(out, lua_typename(state, type), capacity);
        break;
    }
}

static int lua_print(lua_State *state) {
    char line[LUA_CONSOLE_OUTPUT_LINE];
    size_t used = 0;
    int i, count = lua_gettop(state);

    for (i = 1; i <= count && used + 1 < sizeof(line); i++) {
        char value[256];
        size_t length;

        if (i > 1) {
            line[used++] = '\t';
        }
        format_value(state, i, value, sizeof(value));
        length = strlen(value);
        if (length > sizeof(line) - used - 1) {
            length = sizeof(line) - used - 1;
        }
        memcpy(line + used, value, length);
        used += length;
    }
    line[used] = 0;
    output_text(line);
    return 0;
}

static int lua_help(lua_State *state) {
    (void)state;
    output_text("Lua 5.4 console. addBits(amount): signed integer adjustment, clamped to 0..99,999,999.");
    output_text("printPartyList() lists party Digimon; changeDP(id, amount) adjusts a unique party ID's DP (0..99).");
    output_text("addExp(id, amount) adjusts EXP (0..99,999,999); addMaxLevel(id, amount) adjusts max level.");
    output_text("giveItem(name, count) adds a source-defined item for testing (e.g. giveItem('InfMissile', 1)).");
    output_text("Examples: addBits(500), changeDP(10, -1), addExp(10, 500), addMaxLevel(10, 1)");
    return 0;
}

static int enqueue_game_request(lua_State *state, int kind, int digi_id, uint32_t item_key, int64_t amount) {
    GameRequest *request;
    int at;

    if (lock == NULL) {
        return luaL_error(state, "console is not initialized");
    }
    SDL_LockMutex(lock);
    if (queue_count == LUA_CONSOLE_QUEUE) {
        SDL_UnlockMutex(lock);
        return luaL_error(state, "game command queue is full");
    }
    at = (queue_head + queue_count) % LUA_CONSOLE_QUEUE;
    request = &queue[at];
    request->kind = kind;
    request->digi_id = digi_id;
    request->item_key = item_key;
    request->amount = amount;
    queue_count++;
    SDL_UnlockMutex(lock);
    lua_pushboolean(state, 1);
    return 1;
}

static int lua_add_bits(lua_State *state) {
    if (lua_gettop(state) != 1 || !lua_isinteger(state, 1)) {
        return luaL_error(state, "addBits expects one integer amount");
    }
    return enqueue_game_request(state, GAME_REQUEST_ADD_BITS, 0, 0, (int64_t)lua_tointeger(state, 1));
}

static int lua_print_party_list(lua_State *state) {
    if (lua_gettop(state) != 0) {
        return luaL_error(state, "printPartyList expects no arguments");
    }
    return enqueue_game_request(state, GAME_REQUEST_PRINT_PARTY, 0, 0, 0);
}

static int lua_change_dp(lua_State *state) {
    lua_Integer id, amount;

    if (lua_gettop(state) != 2 || !lua_isinteger(state, 1) || !lua_isinteger(state, 2)) {
        return luaL_error(state, "changeDP expects a Digimon id and an integer adjustment");
    }
    id = lua_tointeger(state, 1);
    if (id < 0 || id > 255) {
        return luaL_error(state, "changeDP Digimon id must be in the range 0..255");
    }
    amount = lua_tointeger(state, 2);
    return enqueue_game_request(state, GAME_REQUEST_CHANGE_DP, (int)id, 0, (int64_t)amount);
}

static int lua_add_exp(lua_State *state) {
    lua_Integer id, amount;

    if (lua_gettop(state) != 2 || !lua_isinteger(state, 1) || !lua_isinteger(state, 2)) {
        return luaL_error(state, "addExp expects a Digimon id and an integer amount");
    }
    id = lua_tointeger(state, 1);
    if (id < 0 || id > 255) {
        return luaL_error(state, "addExp Digimon id must be in the range 0..255");
    }
    amount = lua_tointeger(state, 2);
    return enqueue_game_request(state, GAME_REQUEST_ADD_EXP, (int)id, 0, (int64_t)amount);
}

static int lua_add_max_level(lua_State *state) {
    lua_Integer id, amount;

    if (lua_gettop(state) != 2 || !lua_isinteger(state, 1) || !lua_isinteger(state, 2)) {
        return luaL_error(state, "addMaxLevel expects a Digimon id and an integer amount");
    }
    id = lua_tointeger(state, 1);
    if (id < 0 || id > 255) {
        return luaL_error(state, "addMaxLevel Digimon id must be in the range 0..255");
    }
    amount = lua_tointeger(state, 2);
    return enqueue_game_request(state, GAME_REQUEST_ADD_MAX_LEVEL, (int)id, 0, (int64_t)amount);
}

static int lua_give_item(lua_State *state) {
    const char *name;
    lua_Integer count;
    u32 key;

    if (lua_gettop(state) != 2 || lua_type(state, 1) != LUA_TSTRING || !lua_isinteger(state, 2)) {
        return luaL_error(state, "giveItem expects a source item name and an integer count");
    }
    name = lua_tostring(state, 1);
    key = ItemExtension_GetKeyByName(name);
    if (key == 0) {
        return luaL_error(state, "giveItem: unknown source item '%s'", name);
    }
    count = lua_tointeger(state, 2);
    if (count < 1 || count > 48) {
        return luaL_error(state, "giveItem count must be in the range 1..48");
    }
    return enqueue_game_request(state, GAME_REQUEST_GIVE_ITEM, 0, key, (int64_t)count);
}

static void append_name_text(char *out, size_t capacity, size_t *used, const char *text) {
    while (*text != 0 && *used + 1 < capacity) {
        out[(*used)++] = *text++;
    }
    out[*used] = 0;
}

/* Digi_GetDefaultName returns the game's compact glyph bytes; party names use the same font
 * encoding as save nicknames. Decode the printable glyphs here so this remains available in
 * release builds (the dev-data text decoder is intentionally dev-only). */
static void decode_digi_name(const uint8_t *raw, size_t raw_capacity, char *out, size_t capacity) {
    static const char *const punctuation[] = {
        "(O)", "(X)", "(Tri)", "(Sq)", "&", "II", "?", "!", "/", "(note)", "·", "-",
        "L1", "L2", "L3", "L4", "L5", "", "", "", "", ">", ",", ".", "'", "\"", ";", ":", "%", "+", "=", "#"
    };
    size_t used = 0;
    int i;

    if (capacity == 0) {
        return;
    }
    out[0] = 0;
    if (raw == NULL) {
        SDL_strlcpy(out, "?", capacity);
        return;
    }
    for (i = 0; (size_t)i < raw_capacity && raw[i] != 0xFF; i++) {
        unsigned int c = raw[i];
        char glyph[2] = { 0, 0 };
        const char *text = NULL;

        if (c < 10) {
            glyph[0] = (char)('0' + c);
            text = glyph;
        } else if (c < 0x24) {
            glyph[0] = (char)('A' + c - 0x0A);
            text = glyph;
        } else if (c < 0x3E) {
            glyph[0] = (char)('a' + c - 0x24);
            text = glyph;
        } else if (c <= 0x5D) {
            text = punctuation[c - 0x3E];
        } else if (c == 0xFD || c == 0xFE || c == 0xFC) {
            text = " ";
        } else if (c == 0xEF && (size_t)(i + 1) < raw_capacity && raw[i + 1] != 0xFF) {
            i++; /* extended glyphs are outside the printable Digimon-name alphabet */
            text = "?";
        } else {
            text = "?";
        }
        append_name_text(out, capacity, &used, text);
    }
    if (out[0] == 0) {
        SDL_strlcpy(out, "?", capacity);
    }
}

static void instruction_hook(lua_State *state, lua_Debug *debug) {
    (void)debug;
    instructions_left -= LUA_CONSOLE_HOOK_GRANULARITY;
    if (instructions_left <= 0) {
        luaL_error(state, "instruction budget exceeded");
    }
}

static int execute_loaded_chunk(int expression) {
    int status;

    instructions_left = LUA_CONSOLE_INSTRUCTION_BUDGET;
    lua_sethook(lua, instruction_hook, LUA_MASKCOUNT, LUA_CONSOLE_HOOK_GRANULARITY);
    status = lua_pcall(lua, 0, LUA_MULTRET, 0);
    lua_sethook(lua, NULL, 0, 0);
    if (status != LUA_OK) {
        const char *message = lua_tostring(lua, -1);
        output_text(message != NULL ? message : "Lua execution failed");
        lua_settop(lua, 0);
        return 0;
    }
    if (expression) {
        int i, count = lua_gettop(lua);
        char line[LUA_CONSOLE_OUTPUT_LINE];
        size_t used = 0;

        for (i = 1; i <= count && used + 1 < sizeof(line); i++) {
            char value[256];
            size_t length;

            if (i > 1) {
                line[used++] = '\t';
            }
            format_value(lua, i, value, sizeof(value));
            length = strlen(value);
            if (length > sizeof(line) - used - 1) {
                length = sizeof(line) - used - 1;
            }
            memcpy(line + used, value, length);
            used += length;
        }
        if (count > 0) {
            line[used] = 0;
            output_text(line);
        }
    }
    lua_settop(lua, 0);
    return 1;
}

static int load_chunk(const char *source, size_t length) {
    return luaL_loadbufferx(lua, source, length, "=console", "t");
}

void LuaConsole_SetEnabled(int on) {
    enabled = on != 0;
}

int LuaConsole_Enabled(void) {
    return enabled && lua != NULL;
}

int LuaConsole_Init(void) {
    if (!enabled) {
        return 1;
    }
    lock = SDL_CreateMutex();
    if (lock == NULL) {
        printf("[lua] console disabled: could not create mutex (%s)\n", SDL_GetError());
        enabled = 0;
        return 0;
    }
    lua_memory = 0;
    lua = lua_newstate(lua_allocator, NULL);
    if (lua == NULL) {
        printf("[lua] console disabled: could not create Lua state\n");
        SDL_DestroyMutex(lock);
        lock = NULL;
        enabled = 0;
        return 0;
    }

    luaL_requiref(lua, "_G", luaopen_base, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(lua, 1);
    luaL_requiref(lua, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(lua, 1);

    /* Keep the convenient base library, but no file loading, nested protected calls, or GC control. */
    {
        static const char *const removed[] = { "dofile", "loadfile", "load", "pcall", "xpcall", "collectgarbage", NULL };
        int i;

        for (i = 0; removed[i] != NULL; i++) {
            lua_pushnil(lua);
            lua_setglobal(lua, removed[i]);
        }
    }
    lua_pushcfunction(lua, lua_print);
    lua_setglobal(lua, "print");
    lua_pushcfunction(lua, lua_help);
    lua_setglobal(lua, "help");
    lua_pushcfunction(lua, lua_add_bits);
    lua_setglobal(lua, "addBits");
    lua_pushcfunction(lua, lua_print_party_list);
    lua_setglobal(lua, "printPartyList");
    lua_pushcfunction(lua, lua_change_dp);
    lua_setglobal(lua, "changeDP");
    lua_pushcfunction(lua, lua_add_exp);
    lua_setglobal(lua, "addExp");
    lua_pushcfunction(lua, lua_add_max_level);
    lua_setglobal(lua, "addMaxLevel");
    lua_pushcfunction(lua, lua_give_item);
    lua_setglobal(lua, "giveItem");
    output_text("Lua 5.4.8 ready. Type help() for available game functions.");
    printf("[lua] console ready (Lua 5.4.8, --luaconsole, `)\n");
    return 1;
}

void LuaConsole_Shutdown(void) {
    is_open = 0;
    if (lua != NULL) {
        lua_close(lua);
        lua = NULL;
    }
    if (lock != NULL) {
        SDL_DestroyMutex(lock);
        lock = NULL;
    }
}

int LuaConsole_IsOpen(void) {
    return LuaConsole_Enabled() && is_open;
}

void LuaConsole_SetOpen(int on) {
    if (LuaConsole_Enabled()) {
        is_open = on != 0;
    }
}

void LuaConsole_Submit(const char *source) {
    char prompt[LUA_CONSOLE_INPUT_MAX + 3];
    char expression[LUA_CONSOLE_INPUT_MAX + 8];
    size_t length;
    int status;

    if (!LuaConsole_Enabled() || source == NULL) {
        return;
    }
    length = strlen(source);
    if (length == 0) {
        return;
    }
    if (length > LUA_CONSOLE_INPUT_MAX) {
        output_text("input is too long");
        return;
    }
    SDL_snprintf(prompt, sizeof(prompt), "> %s", source);
    output_text(prompt);
    lua_settop(lua, 0);

    SDL_snprintf(expression, sizeof(expression), "return %s", source);
    status = load_chunk(expression, strlen(expression));
    if (status == LUA_OK) {
        (void)execute_loaded_chunk(1);
        return;
    }
    lua_settop(lua, 0); /* not an expression; try the input as a statement/chunk */
    status = load_chunk(source, length);
    if (status != LUA_OK) {
        const char *message = lua_tostring(lua, -1);
        output_text(message != NULL ? message : "Lua syntax error");
        lua_settop(lua, 0);
        return;
    }
    (void)execute_loaded_chunk(1);
}

void LuaConsole_Apply(void) {
    for (;;) {
        GameRequest request;
        int before, after;
        int64_t current;

        if (lock == NULL) {
            return;
        }
        SDL_LockMutex(lock);
        if (queue_count == 0) {
            SDL_UnlockMutex(lock);
            return;
        }
        request = queue[queue_head];
        queue_head = (queue_head + 1) % LUA_CONSOLE_QUEUE;
        queue_count--;
        SDL_UnlockMutex(lock);

        if (request.kind == GAME_REQUEST_ADD_BITS) {
            before = Save_GameState.bits;
            current = before < 0 ? 0 : before > GAME_BITS_MAX ? GAME_BITS_MAX : before;
            if (request.amount > GAME_BITS_MAX - current) {
                after = GAME_BITS_MAX;
            } else if (request.amount < -current) {
                after = 0;
            } else {
                after = (int)(current + request.amount);
            }
            Save_GameState.bits = after;
            {
                char message[128];
                SDL_snprintf(message, sizeof(message), "addBits(%+lld) applied: bits %d -> %d",
                             (long long)request.amount, before, after);
                output_text(message);
            }
        } else if (request.kind == GAME_REQUEST_PRINT_PARTY) {
            int i, count = 0;

            output_text("Party Digimon:");
            for (i = 0; i < (int)(sizeof(Save_GameState.elems) / sizeof(Save_GameState.elems[0])); i++) {
                DigiRosterEntry *entry = &Save_GameState.elems[i];
                uint8_t *raw_name;
                char name[64];
                char line[128];

                if (entry->state < 3 || entry->state > 5) {
                    continue;
                }
                raw_name = entry->name;
                decode_digi_name(raw_name, sizeof(entry->name), name, sizeof(name));
                if (entry->name[0] == 0xFF || entry->name[0] == 0) {
                    raw_name = Digi_GetDefaultName(entry->digiId);
                    decode_digi_name(raw_name, sizeof(entry->name), name, sizeof(name));
                }
                SDL_snprintf(line, sizeof(line), "%s - %u", name, (unsigned)entry->digiId);
                output_text(line);
                count++;
            }
            if (count == 0) {
                output_text("(party is empty)");
            }
        } else if (request.kind == GAME_REQUEST_CHANGE_DP || request.kind == GAME_REQUEST_ADD_EXP ||
                   request.kind == GAME_REQUEST_ADD_MAX_LEVEL) {
            int i, found = -1, matches = 0;
            const char *operation = request.kind == GAME_REQUEST_CHANGE_DP
                                        ? "changeDP"
                                        : request.kind == GAME_REQUEST_ADD_EXP ? "addExp" : "addMaxLevel";

            for (i = 0; i < (int)(sizeof(Save_GameState.elems) / sizeof(Save_GameState.elems[0])); i++) {
                DigiRosterEntry *entry = &Save_GameState.elems[i];

                if (entry->state < 3 || entry->state > 5 || entry->digiId != request.digi_id) {
                    continue;
                }
                found = i;
                matches++;
            }
            if (matches == 1) {
                DigiRosterEntry *entry = &Save_GameState.elems[found];
                if (request.kind == GAME_REQUEST_CHANGE_DP) {
                    int before = entry->dp, after;

                    if (before > 99) {
                        before = 99;
                    }
                    if (request.amount > 99 - before) {
                        after = 99;
                    } else if (request.amount < -before) {
                        after = 0;
                    } else {
                        after = (int)(before + request.amount);
                    }
                    entry->dp = (uint8_t)after;
                    {
                        char message[128];
                        SDL_snprintf(message, sizeof(message), "%s(%u, %+lld) applied: DP %d -> %d",
                                     operation, (unsigned)request.digi_id, (long long)request.amount, before, after);
                        output_text(message);
                    }
                } else if (request.kind == GAME_REQUEST_ADD_EXP) {
                    int before = entry->exp, current_exp, after;

                    current_exp = before < 0 ? 0 : before > GAME_EXP_MAX ? GAME_EXP_MAX : before;
                    if (request.amount > GAME_EXP_MAX - current_exp) {
                        after = GAME_EXP_MAX;
                    } else if (request.amount < -current_exp) {
                        after = 0;
                    } else {
                        after = (int)(current_exp + request.amount);
                    }
                    entry->exp = after;
                    {
                        char message[128];
                        SDL_snprintf(message, sizeof(message), "%s(%u, %+lld) applied: EXP %d -> %d",
                                     operation, (unsigned)request.digi_id, (long long)request.amount, before, after);
                        output_text(message);
                    }
                } else {
                    int before = entry->maxLevel;
                    int min_level = entry->level < 1 ? 1 : entry->level > GAME_MAX_LEVEL ? GAME_MAX_LEVEL : entry->level;
                    int current_max = before < min_level ? min_level : before > GAME_MAX_LEVEL ? GAME_MAX_LEVEL : before;
                    int after;

                    if (request.amount > GAME_MAX_LEVEL - current_max) {
                        after = GAME_MAX_LEVEL;
                    } else if (request.amount < min_level - current_max) {
                        after = min_level;
                    } else {
                        after = (int)(current_max + request.amount);
                    }
                    entry->maxLevel = (uint8_t)after;
                    {
                        char message[128];
                        SDL_snprintf(message, sizeof(message), "%s(%u, %+lld) applied: maxLevel %d -> %d",
                                     operation, (unsigned)request.digi_id, (long long)request.amount, before, after);
                        output_text(message);
                    }
                }
            } else if (matches == 0) {
                char message[96];
                SDL_snprintf(message, sizeof(message), "%s: Digimon id %u is not in the party", operation,
                             (unsigned)request.digi_id);
                output_text(message);
            } else {
                char message[128];
                SDL_snprintf(message, sizeof(message),
                             "%s: Digimon id %u appears more than once in the party; no change made", operation,
                             (unsigned)request.digi_id);
                output_text(message);
            }
        } else if (request.kind == GAME_REQUEST_GIVE_ITEM) {
            const ItemExtension *item = ItemExtension_FindByKey(request.item_key);
            int added = 0, i;

            if (item == NULL || item->runtimeId == 0) {
                output_text("giveItem: source item is unavailable");
                continue;
            }
            for (i = 0; i < request.amount; i++) {
                if (Item_AddToBag(item->runtimeId) < 0) {
                    break;
                }
                added++;
            }
            if (added != 0) {
                Item_SortList();
            }
            {
                char message[128];
                SDL_snprintf(message, sizeof(message), "giveItem('%s', %d) added %d to the bag",
                             item->name, (int)request.amount, added);
                output_text(message);
            }
        }
    }
}
