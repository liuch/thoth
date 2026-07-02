#include "lua.h" // IWYU pragma: keep
#include "http/module.h"
#include "html/html.h"
#include "json/json.h"
#include "utils/utils.h"
#include "readline/readline.h"

/**
 * Returns a key-value list of the available modules
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - table Table to add modules to
 *
 * Output:
 *   nothing
 */
void get_modules(lua_State *L)
{
	http_init(L);
	lua_setfield(L, -2, "http");

	html_init(L);
	lua_setfield(L, -2, "html");

	json_init(L);
	lua_setfield(L, -2, "json");

	readline_init(L);
	lua_setfield(L, -2, "readline");

	utils_init(L);
	lua_setfield(L, -2, "utils");
}
