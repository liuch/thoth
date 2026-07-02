#include "dataset.h"
#include "module.h"

#define COOKIEJAR_ACTIVE_KEY "active_cjar_key"
#define COOKIEJAR_ACTIVE_PUB "active_cjar_glb"
#define COOKIEJAR_NEW_KEY    "new_cjar_key"
#define COOKIEJAR_NEW_PUB    "new_cjar_glb"
#define COOKIEJAR_LOCAL_TAB  "cjar_local"
#define COOKIEJAR_GLOBAL_TAB "cjar_global"

/**
 * Fills the table with default values
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Socket data set table
 *
 * Output:
 *   nothing
 */
void dataset_init(lua_State *L)
{
	lua_pushboolean(L, false);
	lua_setfield(L, -2, COOKIEJAR_ACTIVE_PUB);
}

/**
 * Clears the data set table
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Socket data set table
 *
 * Output:
 *   nothing
 */
void dataset_clear(lua_State *L)
{
	const char *params[] = {
		COOKIEJAR_ACTIVE_KEY,
		COOKIEJAR_ACTIVE_PUB,
		COOKIEJAR_NEW_KEY,
		COOKIEJAR_NEW_PUB,
		COOKIEJAR_LOCAL_TAB,
		COOKIEJAR_GLOBAL_TAB
	};
	const int types[] = {
		LUA_TNIL,
		LUA_TBOOLEAN,
		LUA_TNIL,
		LUA_TBOOLEAN,
		LUA_TNIL,
		LUA_TNIL
	};
	for (size_t i = 0; i < sizeof(params) / sizeof(params[0]); ++i) {
		lua_pushstring(L, params[i]);
		switch (types[i]) {
			case LUA_TBOOLEAN:
				lua_pushboolean(L, false);
				break;
			case LUA_TNIL:
			default:
				lua_pushnil(L);
				break;
		}
		lua_rawset(L, -3);
	}
}

/**
 * Returns the module object bound to the socket
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Socket data set table
 *
 * Output:
 *   nothing
 */
void dataset_get_module(lua_State *L)
{
	lua_getfield(L, -1, "module");
}

/**
 * Checks whether cookies are enabled
 *
 * @param lua_State* L Lua stack
 *
 * @return bool
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Socket data set table
 *
 * Output:
 *   nothing
 *
 */
bool dataset_is_cookiejar_active(lua_State *L)
{
	bool result = (lua_getfield(L, -1, COOKIEJAR_ACTIVE_KEY) != LUA_TNIL);
	lua_pop(L, 1);
	return result;
}

/**
 * Returns true if the socket uses global cookies
 *
 * @param lua_State* L Lua stack
 *
 * @return bool
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Socket data set table
 *
 * Output:
 *   nothing
 *
 */
bool dataset_is_cookiejar_global(lua_State *L)
{
	lua_getfield(L, -1, COOKIEJAR_ACTIVE_PUB);
	bool result = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return result;
}

/**
 * Sets a new active cookie jar
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table    Socket data set
 *   - any      Cookie jar key
 *   - boolean  Whether the cookie jar is global
 *
 * Output:
 *   nothing
 */
void dataset_set_new_cookiejar_params(lua_State *L)
{
	lua_pushvalue(L, -2);
	lua_setfield(L, -4, COOKIEJAR_NEW_KEY);
	lua_pushvalue(L, -1);
	lua_setfield(L, -4, COOKIEJAR_NEW_PUB);
}

/**
 * Returns the key and global flag of the active cookie jar
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table    Socket data set
 *
 * Output:
 *   - any      Cookie jar key
 *   - boolean  Whether the cookie jar is global
 */
void dataset_get_active_cookiejar_params(lua_State *L)
{
	lua_getfield(L, -1, COOKIEJAR_ACTIVE_KEY);
	lua_getfield(L, -2, COOKIEJAR_ACTIVE_PUB);
}

/**
 * Returns the active cookie jar table on the top of the stack
 *
 * @param lua_State* L           Lua stack
 * @param int        dataset_pos Stack position pointing to the socket data set
 * @param bool       empty       If true, it returns a new empty jar table
 *
 * @return int Type of the output data
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - table|nil Cookie storage table
 */
int dataset_get_cookiejar(lua_State *L, int dataset_pos, bool empty)
{
	if (lua_getfield(L, dataset_pos, COOKIEJAR_ACTIVE_KEY) == LUA_TNIL) {
		return LUA_TNIL;
	}

	int pos1 = lua_gettop(L); // Cookiejar key

	lua_getfield(L, dataset_pos, COOKIEJAR_ACTIVE_PUB);
	if (lua_toboolean(L, -1)) {
		if (lua_getfield(L, dataset_pos, COOKIEJAR_GLOBAL_TAB) == LUA_TNIL) {
			lua_getfield(L, dataset_pos, "module");
			module_get_cookiejar_storage(L);
			lua_pushvalue(L, -1);
			lua_setfield(L, dataset_pos, COOKIEJAR_GLOBAL_TAB);
		}
	} else {
		luaL_getsubtable(L, dataset_pos, COOKIEJAR_LOCAL_TAB);
	}

	if (empty) {
		lua_newtable(L);
		lua_pushvalue(L, pos1);
		lua_pushvalue(L, -2);
		lua_rawset(L, -4);
	} else {
		lua_pushvalue(L, pos1);
		lua_rawget(L, -2);
	}

	lua_replace(L, pos1);
	lua_settop(L, pos1);
	return lua_type(L, pos1);
}

/**
 * Replaces the current cookie jar key and global flag with new ones
 *
 * @param lua_State* L           Lua stack
 * @param int        dataset_pos Socket dataset position
 *
 * @return bool True, if the jar was actually changed
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
bool dataset_apply_new_cookiejar_key(lua_State *L, int dataset_pos)
{
	int top = lua_gettop(L);
	bool result = false;

	lua_getfield(L, dataset_pos, COOKIEJAR_ACTIVE_KEY);
	lua_getfield(L, dataset_pos, COOKIEJAR_ACTIVE_PUB);
	if (lua_getfield(L, dataset_pos, COOKIEJAR_NEW_KEY) != LUA_TNIL) {
		lua_getfield(L, dataset_pos, COOKIEJAR_NEW_PUB);
	} else {
		lua_pushboolean(L, false);
	}
	if (lua_compare(L, -4, -2, LUA_OPEQ) == 0 || lua_compare(L, -3, -1, LUA_OPEQ) == 0) {
		lua_setfield(L, dataset_pos, COOKIEJAR_ACTIVE_PUB);
		lua_setfield(L, dataset_pos, COOKIEJAR_ACTIVE_KEY);
		result = true;
	}
	lua_pushnil(L);
	lua_setfield(L, dataset_pos, COOKIEJAR_NEW_KEY);
	lua_pushnil(L);
	lua_setfield(L, dataset_pos, COOKIEJAR_NEW_PUB);

	lua_settop(L, top);
	return result;
}
