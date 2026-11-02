#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <b64/cencode.h>
#include <b64/cdecode.h>
#include "../lua.h"

#define UTILS_MODULE_META_NAME "thoth.utils"

struct utils_data {
	bool active;
};

static int module_started = 0;

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
		luaL_error(L, "The function expects 1 argument, got %d", cnt);
	}

	size_t len;
	const char *start = luaL_checklstring(L, 1, &len);

	const char *end = start + len;

	for ( ; start < end; ++start) {
		if (!isspace((unsigned char)*start)) break;
	}
	for ( ; start < end; --end) {
		if (!isspace((unsigned char)end[-1])) break;
	}

	lua_pushlstring(L, start, (size_t)(end - start));
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
		lua_pushfstring(L, "The function expects 1 argument, %d passed", cnt);
		lua_error(L);
	}

	size_t str_len;
	const char *str = luaL_checklstring(L, 1, &str_len);
	luaL_Buffer buf;
	size_t buf_size = str_len * 3 / 4 + 1;
	char *res = luaL_buffinitsize(L, &buf, buf_size);

	base64_decodestate state;
	base64_init_decodestate(&state);

	size_t dec_bytes = 0;
	size_t max_bytes = INT_MAX / 2;

	for (size_t offset = 0; offset < str_len; ) {
		size_t bytes_to_decode = str_len - offset;
		if (bytes_to_decode > max_bytes) bytes_to_decode = max_bytes;
		size_t res_bytes = (size_t)base64_decode_block(
			str + offset,
			(int)bytes_to_decode,
			res + dec_bytes,
			&state
		);
		offset += bytes_to_decode;
		dec_bytes += res_bytes;
	}

	assert(buf_size >= dec_bytes);

	luaL_pushresultsize(&buf, dec_bytes);

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
		lua_pushfstring(L, "The function expects 1 argument, %d passed", cnt);
		return lua_error(L);
	}

	size_t str_len;
	const char *str = luaL_checklstring(L, 1, &str_len);
	luaL_Buffer buf;
	size_t buf_size = 4 * (str_len + 2) / 3 + 1;
	char *res = luaL_buffinitsize(L, &buf, buf_size);

	base64_encodestate state;
	base64_init_encodestate(&state);

	size_t enc_bytes = 0;
	size_t max_bytes = INT_MAX / 2;

	for (size_t offset = 0; offset < str_len; ) {
		size_t bytes_to_encode = str_len - offset;
		if (bytes_to_encode > max_bytes) bytes_to_encode = max_bytes;
		size_t res_bytes = (size_t)base64_encode_block(
			str + offset,
			(int)bytes_to_encode,
			res + enc_bytes,
			&state
		);
		offset += bytes_to_encode;
		enc_bytes += res_bytes;
	}
	enc_bytes += (size_t)base64_encode_blockend(res + enc_bytes, &state);

	assert(buf_size >= enc_bytes);

	luaL_pushresultsize(&buf, enc_bytes);

	lua_replace(L, 1);
	lua_settop(L, 1);

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
	const struct utils_data *udata = luaL_testudata(L, 1, UTILS_MODULE_META_NAME);
	if (udata && udata->active) {
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
 * @return 1
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

	luaL_checkudata(L, 1, UTILS_MODULE_META_NAME);

	if (lua_type(L, 2) != LUA_TSTRING) {
		lua_pushnil(L);
		return 1;
	}

	if (get_cfunction(L, lua_tostring(L, 2), flist)) {
		return 1;
	}

	lua_pushnil(L);
	return 1;
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
