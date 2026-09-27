#include <stdlib.h>
#include <stdbool.h>
#include <jansson.h>
#include "../lua.h"
#include "json.h"

#define JSON_MODULE_META_NAME "thoth.json"
#define JSON_VALUE_META_NAME  "thoth.json.value"

struct json_data {
	bool active;
};

static int make_json_value(lua_State *L, json_t **storage);
static int iterate_array(lua_State *L, json_t *array);
static int iterate_object(lua_State *L, json_t *object);

static int module_started = 0;

/**
 * Frees up resources allocated by JSON library
 *
 * The call is performed by the Lua garbage collector.
 * This approach allows correct releasing of resources then errors occurs.
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata JSON data
 *
 * Output:
 *   nothing
 */
static int json_value_cleanup(lua_State *L)
{
	json_t **d = lua_touserdata(L, 1);
	if (*d != NULL) {
		json_decref(*d);
		*d = NULL;
	}
	return 0;
}

/**
 * Creates a lua value corresponding to the passed structure
 * and pushes the value on the Lua stack
 *
 * @param lua_State* L     Lua stack
 * @param json_t*    value JSON library structure
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   noting
 *
 * Output:
 *   - table|string|number|boolean|nil Lua value converted from the JSON library structure
 *   - string|nil                      Error message in case of an error
 */
static int push_json_value(lua_State *L, json_t *value)
{
	int rcnt = 1;
	switch (json_typeof(value)) {
		case JSON_ARRAY:
			luaL_checkstack(L, 3, "JSON: too many nested elements");
			lua_newtable(L);
			rcnt = iterate_array(L, value) + 1;
			break;
		case JSON_OBJECT:
			luaL_checkstack(L, 3, "JSON: too many nested elements");
			lua_newtable(L);
			rcnt = iterate_object(L, value) + 1;
			break;
		case JSON_STRING:
			lua_pushstring(L, json_string_value(value));
			break;
		case JSON_INTEGER:
			lua_pushinteger(L, json_integer_value(value));
			break;
		case JSON_REAL:
			lua_pushnumber(L, json_real_value(value));
			break;
		case JSON_TRUE:
			lua_pushboolean(L, true);
			break;
		case JSON_FALSE:
			lua_pushboolean(L, false);
			break;
		case JSON_NULL:
			lua_pushnil(L);
			break;
		default:
			lua_pushnil(L);
			lua_pushliteral(L, "JSON: unsupported data type");
			rcnt = 2;
			break;
	}
	return rcnt;
}

/**
 * Creates and returns a JSON table or object based on the keys and values of the passed Lua table
 *
 * @param lua_State* L       Lua stack
 * @param json_t**   storage JSON value storage to save the result to
 *
 * @return int 0 if there are no errors, otherwise 1
 *
 * === Lua stack ===
 *
 * Input:
 *   table Table to convert
 *
 * Output:
 *   nothing
 */
static int make_json_table(lua_State *L, json_t **storage)
{
	int len = luaL_len(L, -1);

	bool is_object = false;
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		int type = lua_type(L, -2);
		switch (type) {
			case LUA_TSTRING:
				is_object = true;
				break;
			case LUA_TNUMBER:
				if (is_object) break;
				if (!lua_isinteger(L, -2)) {
					is_object = true;
					break;
				}
				int key = lua_tointeger(L, -2);
				if (key < 1 || key > len) is_object = true;
				break;
			default:
				lua_pop(L, 2);
				lua_pushfstring(L, "JSON: incorrect key type: %s", lua_typename(L, type));
				return 1;
		}
		lua_pop(L, 1);
	}

	json_t *table = NULL;
	if (is_object) {
		table = json_object();
		lua_pushnil(L);
		while (lua_next(L, -2) != 0) {
			int res;
			json_t *value = NULL;
			if (make_json_value(L, &value) != 0) {
				json_decref(value);
				return 1;
			}
			lua_pop(L, 1);
			if (lua_type(L, -1) == LUA_TSTRING) {
				res = json_object_set_new(table, lua_tostring(L, -1), value);
			} else {
				lua_pushvalue(L, -1);
				res = json_object_set_new(table, lua_tostring(L, -1), value);
				lua_pop(L, 1);
			}
			if (res != 0) {
				json_decref(table);
				json_decref(value);
				lua_pushliteral(L, "JSON: failed to set object property");
				lua_error(L);
			}
		}
	} else {
		table = json_array();
		for (int i = 1; i <= len; ++i) {
			lua_rawgeti(L, -1, i);
			json_t *value = NULL;
			if (make_json_value(L, &value) != 0) {
				json_decref(value);
				return 1;
			}
			lua_pop(L, 1);
			if (json_array_append_new(table, value) != 0) {
				json_decref(table);
				json_decref(value);
				lua_pushliteral(L, "JSON: failed to append array item");
				lua_error(L);
			}
		}
	}
	*storage = table;
	return 0;
}

/**
 * Creates and returns a JSON value based on the passed Lua value
 *
 * @param lua_State* L       Lua stack
 * @param json_t**   storage JSON value storage to save the result to
 *
 * @return int 0 if there are no errors, otherwise 1
 *
 * === Lua stack ===
 *
 * Input:
 *   table|string|number|boolean|nil Value to convert
 *
 * Output:
 *   nothing
 */
static int make_json_value(lua_State *L, json_t **storage)
{
	int type = lua_type(L, -1);
	switch (type) {
		case LUA_TNUMBER:
			if (lua_isinteger(L, -1)) {
				*storage = json_integer(lua_tointeger(L, -1));
			} else {
				*storage = json_real(lua_tonumber(L, -1));
			}
			break;
		case LUA_TSTRING:
			*storage = json_string(lua_tostring(L, -1));
			break;
		case LUA_TTABLE:
			return make_json_table(L, storage);
			break;
		case LUA_TBOOLEAN:
			*storage = json_boolean(lua_toboolean(L, -1));
			break;
		case LUA_TNIL:
			*storage = json_null();
			break;
		default:
			lua_pushfstring(L, "JSON: unsupported data type: %s", lua_typename(L, type));
			return 1;
	}
	if (*storage == NULL) {
		lua_pushliteral(L, "JSON: internal error");
		lua_error(L);
	}
	return 0;
}

/**
 * Iterates through the passed JSON array and inserts the data into the lua table at the top of the stack
 *
 * @param lua_State* L     Lua stack
 * @param json_t*    array JSON array
 *
 * @return int 0 if there are no errors, otherwise 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Empty Lua table to insert into
 *
 * Output:
 *   - string|noting Error message in case of an error
 */
static int iterate_array(lua_State *L, json_t *array)
{
	for (size_t i = 0; i < json_array_size(array); ) {
		json_t *value = json_array_get(array, i);
		if (push_json_value(L, value) == 2) {
			lua_replace(L, -2);
			return 1;
		}
		lua_rawseti(L, -2, ++i);
	}
	return 0;
}

/**
 * Iterates through the passed JSON object and stores the data in the lua table at the top of the stack
 *
 * @param lua_State* L      Lua stack
 * @param json_t*    object JSON object
 *
 * @return int 0 if there are no errors, otherwise 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Empty Lua table to store
 *
 * Output:
 *   - string|noting Error message in case of an error
 */
static int iterate_object(lua_State *L, json_t *object)
{
	const char *key;
	json_t *value;
	json_object_foreach(object, key, value) {
		lua_pushstring(L, key);
		if (push_json_value(L, value) == 2) {
			lua_remove(L, -2);
			lua_remove(L, -2);
			return 1;
		}
		lua_settable(L, -3);
	}
	return 0;
}

/**
 * Creates and initiates the structures required for the module to operate
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata JSON module object
 *
 * Output:
 *   nothing
 */
static void ensure_structures(lua_State *L)
{
	struct json_data *jdata = lua_touserdata(L, -1);
	if (!jdata->active) {
		jdata->active = true;
		if (module_started++) return;

		if (luaL_newmetatable(L, JSON_VALUE_META_NAME)) {
			lua_pushcfunction(L, json_value_cleanup);
			lua_setfield(L, -2, "__gc");
		}

		lua_pop(L, 1);
	}
}

/**
 * Makes the JSON value object
 *
 * This object is intended for temporary storage and guaranteed release
 * of the JSON library resources.
 *
 * @param lua_State* L Lua stack
 *
 * @return json_t**
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   userdata JSON value object
 */
static json_t **make_json_value_storage(lua_State *L)
{
	json_t **p = lua_newuserdata(L, sizeof(json_t *));
	*p = NULL;
	luaL_getmetatable(L, JSON_VALUE_META_NAME);
	lua_setmetatable(L, -2);
	return p;
}

/**
 * Encodes a Lua value into a JSON string
 *
 * @param lua_State* L Lua stack
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata                    JSON module object
 *   - [2] table|boolean|string|number Value to encode
 *
 * Output:
 *   - string|nil                      JSON string or nil in case of an error
 *   - string|nothing                  Error message, if any
 */
static int encode(lua_State *L)
{
	int cnt = lua_gettop(L);
	if (cnt != 2) {
		lua_pushfstring(L, "JSON encode: 2 arguments expected, got %d", cnt);
		lua_error(L);
	}
	luaL_checkudata(L, 1, JSON_MODULE_META_NAME);

	lua_pushvalue(L, 1);
	ensure_structures(L);
	lua_pop(L, 1);

	json_t **p_root = make_json_value_storage(L);

	lua_pushvalue(L, 2);
	int rnum = make_json_value(L, p_root);
	if (rnum == 0) {
		char *str = json_dumps(*p_root, JSON_ENCODE_ANY);
		if (str == NULL) {
			lua_pushliteral(L, "JSON encode failed");
			lua_error(L);
		}
		lua_pop(L, 1);
		lua_pushstring(L, str);
		free(str);
		return 1;
	}

	lua_pushnil(L);
	lua_replace(L, -3);

	return 2;
}

/**
 * Decodes a JSON string and returns a Lua value
 *
 * @param lua_State*  L   Lua stack
 * @param const char* str JSON string to decode
 * @param size_t      len JSON string length
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata JSON module structure
 *
 * Output:
 *   - table|boolean|string|number|nil Result or nil in case of an error
 *   - string|nil                      Error message in case of an error
 */
static int decode_string(lua_State *L, const char *str, size_t len)
{
	ensure_structures(L);

	json_error_t error;
	json_t **p_root = make_json_value_storage(L);
	int stor_pos = lua_gettop(L);
	if (len == 0) {
		*p_root = json_loads(str, JSON_DECODE_ANY, &error);
	} else {
		*p_root = json_loadb(str, len, JSON_DECODE_ANY, &error);
	}

	int rcnt;
	if (*p_root != NULL) {
		rcnt = push_json_value(L, *p_root);
	} else {
		lua_pushnil(L);
		lua_pushfstring(L, "JSON decode error: on line %d: %s", error.line, error.text);
		rcnt = 2;
	}
	lua_remove(L, stor_pos);

	return rcnt;
}

/**
 * Creates a new JSON module and decodes a JSON string in a buffer into one of Lua types
 *
 * @param lua_State*  L   Lua stack
 * @param const char* str JSON string to decode
 * @param size_t      len JSON string length
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - table|boolean|string|number|nil Result or nil in case of an error
 *   - string|nil                      Error message in case of an error
 */
int json_decode_string(lua_State *L, const char *str, size_t len)
{
	json_init(L);
	int mod_pos = lua_gettop(L);
	int rcnt = decode_string(L, str, len);
	lua_remove(L, mod_pos);
	return rcnt;
}

/**
 * Decodes a Lua string into one of Lua types
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata JSON module object
 *   - [2] string   JSON string
 *
 * Output:
 *   - table|boolean|string|number
 */
static int decode(lua_State *L)
{
	luaL_checkudata(L, 1, JSON_MODULE_META_NAME);
	const char *str = luaL_checkstring(L, 2);

	lua_insert(L, 1);
	return decode_string(L, str, luaL_len(L, 1));
}

/**
 * Frees the module's service structures
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata JSON module object
 *
 * Output:
 *   nothing
 */
static int json_unload(lua_State *L)
{
	const struct json_data *jdata = lua_touserdata(L, 1);
	if (jdata->active) {
		if (!--module_started) {
			lua_pushliteral(L, JSON_MODULE_META_NAME);
			lua_pushnil(L);
			lua_rawset(L, LUA_REGISTRYINDEX);
			lua_pushliteral(L, JSON_VALUE_META_NAME);
			lua_pushnil(L);
			lua_rawset(L, LUA_REGISTRYINDEX);
		}
	}
	return 0;
}

/**
 * Implementation of the __index method for the module userdata
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata JSON module object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|nil
 */
static int get_module_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "encode", encode },
		{ "decode", decode }
	};

	return get_cfunction(L, luaL_checkstring(L, 2), flist);
}

/**
 * Create and initiates the JSON module object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - userdata JSON module object
 */
int json_init(lua_State *L)
{
	struct json_data *jdata = lua_newuserdata(L, sizeof(struct json_data));
	jdata->active = false;
	if (luaL_newmetatable(L, JSON_MODULE_META_NAME)) {
		lua_pushcfunction(L, json_unload);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, get_module_property);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}
