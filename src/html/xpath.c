#include <assert.h>
#include "xpath.h"
#include "html.h"

#define HTML_XPATH_META_NAME  "thoth.html.xpath"

struct xpath_data {
	xmlXPathObjectPtr obj;
};

/**
 * Creates and returns an empty XPath result object
 *
 * @param lua_State* L            Lua stack
 * @param int        document_pos HTML document position
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - XPath result object
 */
void xpath_create(lua_State *L, int document_pos)
{
	struct xpath_data *xdata = lua_newuserdata(L, sizeof(struct xpath_data));
	xdata->obj = NULL;
	luaL_getmetatable(L, HTML_XPATH_META_NAME);
	lua_setmetatable(L, -2);

	// Bind the XPath result data to the HTML root element
	// This is necessary to prevent the HTML object from being freed by the garbage collector
	// while the XPath result object is still in use.
	lua_pushvalue(L, document_pos);
	lua_setuservalue(L, -2);
}

/**
 * Puts libxml result into XPath result object in the lua stack
 *
 * @param lua_State* L         Lua stack
 * @param int        xpath_pos XPath result position
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
void xpath_set_result(lua_State *L, int xpath_pos, xmlXPathObjectPtr result)
{
	struct xpath_data *xpath = lua_touserdata(L, xpath_pos);
	xpath->obj = result;
}

/**
 * Frees resources of the XPath result structure
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata XPath result object
 *
 * Output:
 *   nothing
 */
static int cleanup(lua_State *L)
{
	struct xpath_data *xdata = luaL_checkudata(L, 1, HTML_XPATH_META_NAME);
	xmlXPathFreeObject(xdata->obj);
	xdata->obj = NULL;
	return 0;
}

/**
 * Returns the type of XPath result
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata XPath result
 *
 * Output:
 *   - string|noting
 */
static int get_result_type(lua_State *L)
{
	struct xpath_data *xdata = luaL_checkudata(L, 1, HTML_XPATH_META_NAME);
	assert(xdata->obj != NULL);

	switch (xdata->obj->type) {
		case XPATH_NODESET:
			lua_pushliteral(L, "nodeset");
			break;
		case XPATH_BOOLEAN:
			lua_pushliteral(L, "boolean");
			break;
		case XPATH_NUMBER:
			lua_pushliteral(L, "number");
			break;
		case XPATH_STRING:
			lua_pushliteral(L, "string");
			break;
		default:
			return 0;
	}
	return 1;
}

/**
 * Returns the number of elements in the node set of an XPath result object
 *
 * When the query result is not a node set it returns 0.
 *
 * @param lua_State *L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata with XPath result
 *
 * Output:
 *   - number
 */
 static int get_result_size(lua_State *L)
{
	struct xpath_data *xdata = luaL_checkudata(L, 1, HTML_XPATH_META_NAME);
	assert(xdata->obj != NULL);

	int res = 0;
	if (xdata->obj->type == XPATH_NODESET && xdata->obj->nodesetval != NULL) {
		res = xdata->obj->nodesetval->nodeNr;
	}

	lua_pushinteger(L, res);
	return 1;
}

/**
 * Makes a table and fills it with node objects from the nodeset
 *
 * @param lua_State     *L           Lua stack
 * @param int           document_pos Absolute stack index where the HTML document is
 * @param xmlNodeSetPtr nodeset      Set of nodes from libxml2
 *
 * @retrun void
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - table with node objects
 */
static void make_nodeset_table(lua_State *L, int document_pos, xmlNodeSetPtr nodeset)
{
	lua_newtable(L);
	if (nodeset == NULL) return;

	for (int i = 0; i < nodeset->nodeNr; ) {
		html_get_node_by_ptr(L, document_pos, nodeset->nodeTab[i]);
		lua_rawseti(L, -2, ++i);
	}
}

/**
 * Returns the specified value or all the values from XPath result object
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata XPath result object
 *   - [2] int|nil  Result object positon (optional)
 *
 * Output:
 *   - boolean|string|number|table|udata|nothing The result
 */
static int get_result_value(lua_State *L)
{
	struct xpath_data *xdata = luaL_checkudata(L, 1, HTML_XPATH_META_NAME);
	assert(xdata->obj != NULL);

	xmlNodeSetPtr nodeset;

	switch (xdata->obj->type) {
		case XPATH_NODESET:
			int res_stack_index = lua_isnoneornil(L, 2) ? 0 : 2;

			lua_getuservalue(L, 1);
			int document_pos = lua_gettop(L);

			nodeset = xdata->obj->nodesetval;
			if (res_stack_index == 0) {
				make_nodeset_table(L, document_pos, nodeset);
				break;
			} else if (lua_isinteger(L, res_stack_index)) {
				if (nodeset != NULL) {
					int num = lua_tointeger(L, 2);
					if (num > 0 && num <= xdata->obj->nodesetval->nodeNr) {
						html_get_node_by_ptr(L, document_pos, nodeset->nodeTab[num - 1]);
						break;
					}
				}
			}
			return 0;
		case XPATH_BOOLEAN:
			lua_pushboolean(L, xdata->obj->boolval);
			break;
		case XPATH_NUMBER:
			lua_pushnumber(L, xdata->obj->floatval);
			break;
		case XPATH_STRING:
			lua_pushstring(L, (const char *)xdata->obj->stringval);
			break;
		default:
			return 0;
	}
	return 1;
}

/**
 * Implementation of the __index method for XPath result userdata
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata XPath result object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction
 */
static int get_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "type", get_result_type },
		{ "size", get_result_size },
		{ "result", get_result_value },
		{ NULL, NULL }
	};

	const char *pname = luaL_checkstring(L, 2);
	return get_cfunction(L, pname, flist);
}

/**
 * Creates and initiates the required structures
 *
 * @param lua_State* L Lua stack
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
void xpath_init_structures(lua_State *L)
{
	if (luaL_newmetatable(L, HTML_XPATH_META_NAME)) {
		lua_pushcfunction(L, cleanup);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, get_property);
		lua_setfield(L, -2, "__index");
	}
	lua_pop(L, 1);
}
