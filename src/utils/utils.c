#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <b64/cencode.h>
#include <b64/cdecode.h>
#include "../lua.h"

#define UTILS_MODULE_META_NAME "thoth.utils"

struct utils_data {
	bool active;
};

static int module_started = 0;

/**
 * Checks if the passed charracter is whitespace
 *
 * @param char c Character to check
 *
 * @return bool
 */
static bool is_space(char c)
{
	return (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v') ? true : false;
}

/**
 * Removes leading and trailing whitespace from the given Lua string
 * and returns the result as a new Lua string
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] string String to trim
 *
 * Output:
 *   - string String without leading or trailing whitespace
 */
static int trim(lua_State *L)
{
	int cnt = lua_gettop(L);
	if (cnt != 1) {
		lua_pushfstring(L, "The function expects 1 parameter, got %d", cnt);
		lua_error(L);
	}
	const char *start = luaL_checkstring(L, 1);

	const char *end = start + strlen(start);
	for ( ; start < end; ++start) {
		if (!is_space(*start)) break;
	}
	for ( ; start < end; --end) {
		if (!is_space(end[-1])) break;
	}

	lua_pushlstring(L, start, end - start);
	return 1;
}

/**
 * Decodes the base64 data from the given Lua string and returns the result as a new Lua string
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] string Base64 data to decode
 *
 * Output:
 *   - string Result string
 */
static int base64dec(lua_State *L)
{
	int cnt = lua_gettop(L);
	if (cnt != 1) {
		lua_pushfstring(L, "The function expects 1 paremeter, %d passed", cnt);
		lua_error(L);
	}
	const char *str = luaL_checkstring(L, 1);
	int str_len = luaL_len(L, 1);

	char *buffer = malloc(str_len * 3 / 4 + 1);
	if (buffer == NULL) {
		throw_out_of_memory(L, "base64dec");
		lua_error(L);
	}

	base64_decodestate state;
	base64_init_decodestate(&state);
	int res_len = base64_decode_block(str, str_len, buffer, &state);

	lua_pushlstring(L, buffer, res_len);
	free(buffer);
	return 1;
}

/**
 * Encodes the data from the given Lua string into a base64 string and returns it as a new Lua string
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] string Data to encode
 *
 * Output:
 *   - string Base64 string
 */
static int base64enc(lua_State *L)
{
	int cnt = lua_gettop(L);
	if (cnt != 1) {
		lua_pushfstring(L, "The function expects 1 paremeter, %d passed", cnt);
		lua_error(L);
	}
	const char *str = luaL_checkstring(L, 1);
	int str_len = luaL_len(L, 1);

	char *buffer = malloc(4 * (str_len + 2) / 3 + 1);
	if (buffer == NULL) {
		throw_out_of_memory(L, "base64enc");
		lua_error(L);
	}

	base64_encodestate state;
	base64_init_encodestate(&state);

	int res_len = base64_encode_block(str, str_len, buffer, &state);
	res_len += base64_encode_blockend(buffer + res_len, &state);

	lua_pop(L, 1);
	lua_pushlstring(L, buffer, res_len);
	free(buffer);
	return trim(L);
}

/**
 * Frees the module's service structures
 *
 * @param lua_State* L Lua stack
 *
 * return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Utils module object
 *
 * Output:
 *   nothing
 */
static int utils_unload(lua_State *L)
{
	const struct utils_data *udata = lua_touserdata(L, 1);
	if (udata->active) {
		if (!--module_started) {
			lua_pushliteral(L, UTILS_MODULE_META_NAME);
			lua_pushnil(L);
			lua_rawset(L, LUA_REGISTRYINDEX);
		}
	}
	return 0;
}

/**
 * Implementation of the __index method for the module userdata
 *
 * @param lua_State* L Lua state
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Utils module object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|nil
 */
static int get_module_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "trim", trim },
		{ "base64dec", base64dec },
		{ "base64enc", base64enc },
		{ NULL, NULL }
	};

	return get_cfunction(L, luaL_checkstring(L, 2), flist);
}

/**
 * Create and initiates the JSON module object
 *
 * @param lua_State* L Lua stack
 *
 * return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - userdata Utils module object
 */
int utils_init(lua_State *L)
{
	struct utils_data *udata = lua_newuserdata(L, sizeof(struct utils_data));
	udata->active = true;
	++module_started;
	if (luaL_newmetatable(L, UTILS_MODULE_META_NAME)) {
		lua_pushcfunction(L, utils_unload);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, get_module_property);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}
