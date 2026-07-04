#include <libxml2/libxml/xmlerror.h>
#include "error.h"

/**
 * Throws a lua error with the XML parser message
 *
 * @param lua_State  *L          Lua stack
 * @param const char *err_string Default error message
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
void throw_parser_error(lua_State *L, const char *err_string)
{
	const xmlError *error = xmlGetLastError();
	lua_pushstring(L, error != NULL ? error->message : err_string);
	lua_error(L);
}
