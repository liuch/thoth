#include <stdbool.h>
#include "core.h"
#include "version.h"
#include "settings.h"

/**
 * Throws an error accessing an undeclared global variable
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static int deny_access_undeclared_vars(lua_State *L)
{
	lua_Debug di;
	lua_getstack(L, 1, &di);
	lua_getinfo(L, "Sl", &di);
	lua_pushfstring(
		L,
		"%s:%d: Attempt to access undeclared variable \"%s\"",
		di.source,
		di.currentline,
		lua_tostring(L, 2)
	);
	lua_error(L);
	return 0;
}

/**
 * Implementation of the __index function for the metatable of the global table
 *
 * === Lua stack ===
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * Input:
 *   - [1] table  Global table
 *   - [2] string Name
 *
 * Output:
 *   - nil
 */
static int global_read_access(lua_State *L)
{
	if (get_settings(L)->gprotect) {
		return deny_access_undeclared_vars(L);
	}
	lua_pushnil(L);
	return 1;
}

/**
 * Implementation of the __newindex function for the metatable of the global table
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] table  Global table
 *   - [2] string Name
 *   - [3] any    Value
 *
 * Output:
 *   nothing
 */
static int global_write_access(lua_State *L)
{
	if (get_settings(L)->gprotect) {
		return deny_access_undeclared_vars(L);
	}
	lua_rawset(L, 1);
	return 0;
}

/**
 * Sets metatable for global table to control access global variables
 *
 * @param lua_State* L Lua stack
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
void init_core(lua_State *L)
{
	// Init settings
	init_settings(L);
	// Set metatable for global table
	lua_pushglobaltable(L);
	lua_newtable(L);
	lua_pushcfunction(L, global_read_access);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, global_write_access);
	lua_setfield(L, -2, "__newindex");
	lua_setmetatable(L, -2);
	lua_pop(L, 1);
}

/**
 * Adds the application name to the passed table
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Table to add
 *
 * Output:
 *   nothing
 */
void add_app_name(lua_State *L)
{
	lua_pushliteral(L, APP_NAME);
	lua_setfield(L, -2, "_NAME");
}

/**
 * Adds the application version to the passed table
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Table to add
 *
 * Output:
 *   nothing
 */
void add_app_version(lua_State *L)
{
	lua_pushliteral(L, APP_VERSION);
	lua_setfield(L, -2, "_VERSION");
}

/**
 * Sets and returns the global environment protection status.
 * If no argument is passed, the current protection status is returned.
 * Otherwise, the passed argument is set as the current value and the old value is returned.
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] boolean The new protect status value. Optional
 *
 * Output:
 *   - boolean The current or the old protect status value
 */
int global_protect(lua_State *L)
{
	struct settings *st = get_settings(L);
	bool old_val = st->gprotect;
	if (lua_gettop(L)) {
		luaL_argcheck(L, lua_type(L, 1) == LUA_TBOOLEAN, 1, "boolean expected");
		st->gprotect = lua_toboolean(L, 1);
	}
	lua_pushboolean(L, old_val);
	return 1;
}
