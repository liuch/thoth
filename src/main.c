#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "lua.h" // IWYU pragma: keep
#include "core.h"
#include "version.h"
#include "modules.h"

/**
 * The main function
 */
int main(int argc, char **argv)
{
	const char *script_name = argc >= 2 ? argv[1] : "rc.lua";

	lua_State *L = luaL_newstate();
	if (L == NULL) {
		fprintf(stderr, "Failed to create Lua state\n");
		exit(EXIT_FAILURE);
	}
	luaL_openlibs(L);

	init_core(L);
	lua_pushboolean(L, true);
	global_protect(L);
	lua_pop(L, 1);

	// Load script file
	if (luaL_loadfile(L, script_name) != LUA_OK) {
		fprintf(stderr, "Failed to load script: %s\n", lua_tostring(L, -1));
		lua_close(L);
		exit(EXIT_FAILURE);
	}

	// The app argument
	lua_newtable(L);
	get_modules(L);
	lua_pushcfunction(L, global_protect);
	lua_setfield(L, -2, "globalProtect");
	lua_pushliteral(L, APP_NAME);
	lua_setfield(L, -2, "_NAME");
	lua_pushliteral(L, APP_VERSION);
	lua_setfield(L, -2, "_VERSION");

	// Extra arguments
	int extra_args = 0;
	if (argc >= 3) {
		for (int i = 2; i < argc; ++i) lua_pushstring(L, argv[i]);
		extra_args = argc - 2;
	}

	int res = EXIT_SUCCESS;
	if (lua_pcall(L, 1 + extra_args, 0, 0) != LUA_OK) {
		fprintf(stderr, "Failed to run script: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		res = EXIT_FAILURE;
	}

	lua_close(L);
	exit(res);
}
