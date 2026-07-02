#include <stdbool.h>
#include <assert.h>
#include <curl/curl.h>
#include "../lua.h"
#include "../common.h"
#include "socket.h"
#include "dataset.h"
#include "response.h"

#define HTTP_MODULE_META_NAME "thoth.http"
#define COOKIEJAR_TABLE       "cjar_table"

struct http_data {
	bool curl_started;
};

/**
 * Frees the module's service structures
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP module
 *
 * Output:
 *   nothing
 */
static int module_cleanup(lua_State *L)
{
	const struct http_data *hdata = lua_touserdata(L, 1);
	if (hdata->curl_started) {
		curl_global_cleanup();
	}
	socket_cleanup_structures(L);
	return 0;
}

/**
 * Creates and initiates the structures required for the module to operate
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
static void init_structures(lua_State *L)
{
	socket_init_structures(L);
	response_init_structures(L);
	curl_global_init(CURL_GLOBAL_ALL);
}

/**
 * Stores the last active socket into the module structure and returns the previous one
 *
 * @param lua_State* L          Lua stack
 * @param int        socket_pos Stack position of the socket to set
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - table HTTP module data
 *
 * Output:
 *   - userdata Previous socket object
 */
static void set_last_socket(lua_State *L, int socket_pos)
{
	int result_pos = lua_gettop(L) + 1;

	lua_getfield(L, -1, "sockets");
	lua_getfield(L, -1, "last");
	lua_pushvalue(L, socket_pos);
	lua_setfield(L, -3, "last");

	lua_replace(L, result_pos);
	lua_settop(L, result_pos);
}

/**
 * A handler that runs immediately before a resource request is executed.
 * It updates global cookie jars
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Module object
 *   - [2] userdata Socket object
 *
 * Output:
 *   nothing
 */
static int before_socket_fetch(lua_State *L)
{
	lua_getuservalue(L, 1); // [3] Module dataset
	set_last_socket(L, 2); // [4] Last socket object or nil
	if (lua_isnil(L, -1)) return 0;

	if (lua_compare(L, 2, -1, LUA_OPEQ) == 0) {
		socket_get_dataset(L);
		if (dataset_is_cookiejar_global(L)) {
			// Flush the last socket cookies
			int dset_pos = lua_gettop(L);
			if (dataset_get_cookiejar(L, dset_pos, true) == LUA_TTABLE) {
				socket_flush_cookies(L, 4, dset_pos + 1);
			}
		}

		lua_pushvalue(L, 2);
		socket_get_dataset(L);
		if (dataset_is_cookiejar_global(L)) {
			// Update the current socket cookies
			int dset_pos = lua_gettop(L);
			if (dataset_get_cookiejar(L, dset_pos, false) == LUA_TTABLE) {
				socket_update_cookies(L, 2, dset_pos + 1);
			}
		}
	}

	return 0;
}

/**
 * Returns a new socket object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   [1] userdata HTTP module
 *
 * Output:
 *   - userdata Socket object
 */
static int new_socket(lua_State *L)
{
	struct http_data *hdata = luaL_checkudata(L, 1, HTTP_MODULE_META_NAME);
	if (!hdata->curl_started) {
		init_structures(L);
		hdata->curl_started = true;
	}

	socket_create(L);

	lua_getuservalue(L, -1);
	// Link to module object
	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "module");
	// Set handlers
	lua_pushcfunction(L, before_socket_fetch);
	lua_setfield(L, -2, "beforefetch");
	//--
	lua_pop(L, 1);

	return 1;
}

/**
 * Implementation of the __index method for the module userdata
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP module
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|nil
 */
static int get_module_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "getSocket", new_socket },
		{ NULL, NULL }
	};

	return get_cfunction(L, luaL_checkstring(L, 2), flist);
}

/**
 * Creates and initiates the HTTP module
 *
 * @param lua_State *L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - userdata HTTP module
 */
int http_init(lua_State *L)
{
	struct http_data *hdata = lua_newuserdata(L, sizeof(struct http_data));
	hdata->curl_started = false;

	luaL_newmetatable(L, HTTP_MODULE_META_NAME);
	lua_pushcfunction(L, module_cleanup);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, get_module_property);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, -2);

	lua_newtable(L);
	// Socket activity data
	lua_newtable(L);
	if (luaL_newmetatable(L, WEAK_VALUES_META_NAME)) {
		lua_pushliteral(L, "v");
		lua_setfield(L, -2, "__mode");
	}
	lua_setmetatable(L, -2);
	lua_setfield(L, -2, "sockets");
	// --
	lua_setuservalue(L, -2);

	return 1;
}

/**
 * Returns the global cookie jar storage (key-value table)
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata HTTP module
 *
 * Output:
 *   - table    Global cookie jar storage
 */
void module_get_cookiejar_storage(lua_State *L)
{
	lua_getuservalue(L, -1);
	luaL_getsubtable(L, -1, COOKIEJAR_TABLE);
	lua_replace(L, -1);
}
