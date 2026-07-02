#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "../lua.h"
#include "../common.h"
#include "readline.h"

#define READLINE_MODULE_META_NAME "thoth.readline"
#define READLINE_CURRENT_MODULE   "thoth.readline.current"

struct readline_data {
	bool active;
};

static lua_State *lstate = NULL;
static int module_started = 0;

/**
 * Returns the setting value by name
 *
 * @param lua_State*  L    Lua stack
 * @param const char* name Setting name
 *
 * @return int Type of the value
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata Readline module object
 *
 * Output:
 *   - any Setting value
 */
static int get_settings(lua_State *L, const char *name)
{
	lua_getuservalue(L, -1);
	int res = lua_getfield(L, -1, name);
	lua_replace(L, -2);
	return res;
}

/**
 * Sets the value from the lua stack as a setting with the specified name
 *
 * @param lua_State*  L    Lua stack
 * @param const char* name Setting name
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata Readline module object
 *   - any      New setting value
 *
 * Output:
 *   nothing
 */
static void set_settings(lua_State *L, const char *name)
{
	lua_getuservalue(L, -2);
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, name);
	lua_pop(L, 1);
}

/**
 * Searches for the next matching result for autocompletion in the previously
 * generated Lua table
 *
 * @param const char* text  Text to search for
 * @param int         state Search state
 *
 * @return char*
 */
static char *completion_generator(const char *text, int state)
{
	static size_t index, len;
	if (state == 0) {
		index = 0;
		len = strlen(text);
	}

	lua_State *L = lstate;
	int top = lua_gettop(L);

	lua_getfield(L, LUA_REGISTRYINDEX, READLINE_CURRENT_MODULE);
	get_settings(L, "matches");

	char *res = NULL;
	int ntype;
	while ((ntype = lua_rawgeti(L, -1, ++index)) != LUA_TNIL) {
		if (ntype == LUA_TSTRING) {
			const char *str = lua_tostring(L, -1);
			if (strncmp(text, str, len) == 0) {
				res = my_strdup(str);
				break;
			}
		}
		lua_pop(L, 1);
	}

	lua_settop(L, top);
	return res;
}

/**
 * Implementation of rl_completion_entry_function
 *
 * @param char*
 * @param int
 *
 * @return char* NULL
 */
static char *auto_completion_entry(const char *, int)
{
	return NULL;
}

/**
 * Implementation of readline rl_attempted_completion_function
 *
 * This function generates a Lua table with possible autocomplete options
 * and stores it to to the Lua stack. Then it calls the function to generate
 * the appropriate options.
 *
 * @param const char* text  Text to complete
 * @param int         start Start of the text in rl_line_buffer
 * @param int         end   End of the text in rl_line_buffer
 *
 * @return char** List of strings
 */
static char **auto_completion(const char *text, int start, int end)
{
	lua_State *L = lstate;
	int top = lua_gettop(L);

	lua_getfield(L, LUA_REGISTRYINDEX, READLINE_CURRENT_MODULE);
	switch(get_settings(L, "completer")) {
		case LUA_TNIL:
			lua_pop(L, 1);
			lua_newtable(L);
			break;
		case LUA_TTABLE:
			break;
		case LUA_TFUNCTION:
			lua_pushstring(L, text);
			lua_pushinteger(L, start);
			lua_pushinteger(L, end);
			if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
				lua_error(L);
			}
			if (lua_type(L, -1) != LUA_TTABLE) {
				lua_pop(L, 1);
				lua_newtable(L);
			}
			break;
	}
	set_settings(L, "matches");
	lua_pop(L, 1);

	char **res = rl_completion_matches(text, completion_generator);

	lua_pushnil(L);
	set_settings(L, "matches");

	lua_settop(L, top);
	return res;
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
 *   - userdata Readline module object
 *
 * Output:
 *   nothing
 */
static void ensure_structures(lua_State *L)
{
	struct readline_data *rdata = lua_touserdata(L, -1);
	if (!rdata->active) {
		rdata->active = true;
		if (module_started++) return;

		using_history();
	}
}

/**
 * Gets user input and returns it as a Lua string
 *
 * It returns nothing when gets EOL (user have pressed Ctrl+D, for instance)
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata   Readline module object
 *   - [2] string|nil Prompt
 *
 * Output:
 *   - string|nil User input
 */
static int user_input(lua_State *L)
{
	luaL_checkudata(L, 1, READLINE_MODULE_META_NAME);
	lua_pushvalue(L, 1);

	ensure_structures(L);

	int type = lua_type(L, 2);
	luaL_argcheck(L, type == LUA_TSTRING || type == LUA_TNIL, 2, "string or nil expected");

	rl_completion_entry_function = auto_completion_entry;
	rl_attempted_completion_function = auto_completion;

	lstate = L;
	lua_setfield(L, LUA_REGISTRYINDEX, READLINE_CURRENT_MODULE);

	const char *prompt = type == LUA_TSTRING ? lua_tostring(L, 2) : "";
	char *input = readline(prompt);

	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, READLINE_CURRENT_MODULE);

	if (input == NULL) return 0;

	if (*input) {
		const HIST_ENTRY *last_entry = NULL;
		if (history_length > 0) {
			last_entry = history_get(history_base + history_length - 1);
		}
		if (last_entry == NULL || strcmp(input, last_entry->line) != 0) {
			add_history(input);
		}
	}
	lua_pushstring(L, input);
	free(input);

	return 1;
}

/**
 * Configures the auto-completion feature
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata           Readline module object
 *   - [2] nil|table|function Disable/Results/Handler
 *
 * Output:
 *   noting
 */
static int set_completer(lua_State *L)
{
	luaL_checkudata(L, 1, READLINE_MODULE_META_NAME);
	if (lua_gettop(L) == 1) lua_pushnil(L);

	switch (lua_type(L, 2)) {
		case LUA_TNIL:
		case LUA_TTABLE:
		case LUA_TFUNCTION:
			break;
		default:
			luaL_argcheck(L, false, 2, "table, function or nil expected");
	}
	set_settings(L, "completer");

	return 0;
}

/**
 * Frees the module's service structures
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Readline module object
 *
 * Output:
 *   nothing
 */
static int readline_unload(lua_State *L)
{
	const struct readline_data *rdata = lua_touserdata(L, 1);
	if (rdata->active) {
		if (!--module_started) {
			lua_pushliteral(L, READLINE_MODULE_META_NAME);
			lua_pushnil(L);
			lua_rawset(L, LUA_REGISTRYINDEX);
		}
	}
	return 0;
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
 *   - [1] userdata Readline module object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|nil
 */
static int get_module_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "input", user_input },
		{ "setCompleter", set_completer },
		{ NULL, NULL }
	};

	return get_cfunction(L, luaL_checkstring(L, 2), flist);
}

/**
 * Creates and initiates the readline module
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
 *   - userdata Readline module object
 */
void readline_init(lua_State *L)
{
	struct readline_data *rdata = lua_newuserdata(L, sizeof(struct readline_data));
	rdata->active = false;

	if (luaL_newmetatable(L, READLINE_MODULE_META_NAME)) {
		lua_pushcfunction(L, readline_unload);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, get_module_property);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	// Settings table
	lua_newtable(L);
	lua_setuservalue(L, -2);
}
