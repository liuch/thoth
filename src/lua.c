#include <string.h>
#include "lua.h"

/**
 * Throws a Lua error about out of memory
 *
 * @param lua_State* L      Lua stack
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
int throw_out_of_memory(lua_State *L)
{
	lua_pushliteral(L, "not enough memory");
	return lua_error(L);
}

/**
 * Search for cfunction with the given name and push it on the top of the stack
 *
 * @param lua_State*     L     Lua stack
 * @param const luaLReg* flist Name-value array
 *
 * @param int 0 when no function found and 1 otherwise
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - function|nil
 */
int get_cfunction(lua_State *L, const char *name, const luaL_Reg *flist)
{
	for ( ; flist->name; ++flist) {
		if (strcmp(name, flist->name) == 0) {
			lua_pushcfunction(L, flist->func);
			return 1;
		}
	}

	return 0;
}
