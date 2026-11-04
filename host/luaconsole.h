#ifndef HOST_LUACONSOLE_H
#define HOST_LUACONSOLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --luaconsole: optional, release-and-dev Lua REPL, independent of the F2 dev overlay. */
void LuaConsole_SetEnabled(int on); /* main thread, before Ui_Init */
int LuaConsole_Enabled(void);       /* main thread */
int LuaConsole_Init(void);          /* main thread, after SDL/ImGui init */
void LuaConsole_Shutdown(void);     /* main thread, after the game thread stops */
int LuaConsole_IsOpen(void);         /* main thread */
void LuaConsole_SetOpen(int on);     /* main thread */
void LuaConsole_Submit(const char *source); /* main thread */
uint64_t LuaConsole_CopyOutput(char *out, size_t capacity); /* main thread; returns output version */

/* Game thread, at Host_WaitVBlank: applies queued game mutations between frames. */
void LuaConsole_Apply(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_LUACONSOLE_H */
