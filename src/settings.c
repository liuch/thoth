#include <stdbool.h>
#include "lua.h" // IWYU pragma: keep
#include "settings.h"

#define SETTINGS_REGISTRY_NAME "thoth.settings"

/**
 * Creates and initiates the structures required for the settings
 *
 * @param lua_State *L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
void init_settings(lua_State *L)
{
	// Init settings structure and push it into the registry
	struct settings *st = lua_newuserdata(L, sizeof(struct settings));
	st->gprotect = false;
	lua_setfield(L, LUA_REGISTRYINDEX, SETTINGS_REGISTRY_NAME);
}

/**
 * Returns the global settings
 *
 * @param lua_State* L Lua stack
 *
 * @return struct settings*
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
struct settings *get_settings(lua_State *L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, SETTINGS_REGISTRY_NAME);
	struct settings *st = ((struct settings *)lua_touserdata(L, -1));
	lua_pop(L, 1);
	return st;
}

