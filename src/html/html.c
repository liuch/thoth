#include <stdbool.h>
#include "../lua.h"
#include "html.h"
#include "node.h"
#include "xpath.h"
#include "error.h"

#define HTML_MODULE_META_NAME "thoth.html"
#define HTML_DOC_META_NAME    "thoth.html.doc"
#define HTML_NMAP_META_NAME   "thoth.html.node.map"

struct html_data {
	bool active;
};

struct html_doc {
	htmlDocPtr doc;
};

static int module_started = 0;

/**
 * Returns Node object by its pointer in libxml2
 *
 * @param lua_State         *L           Lua stack
 * @param int               document_pos HTML document position
 * @param struct xmlNodePtr node         Node to create the object for
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - userdata Node object
 */
void html_get_node_by_ptr(lua_State *L, int document_pos, xmlNodePtr node)
{
	// Get node map table
	lua_getuservalue(L, document_pos);
	lua_getfield(L, -1, "nodes");
	lua_replace(L, -2);

	// If a lua object has already been created for this node, return it.
	if (lua_rawgetp(L, -1, node) != LUA_TNIL) {
		lua_replace(L, -2);
		return;
	}
	lua_pop(L, 1);

	// Make Node object
	node_create(L, document_pos, node);

	// Set the udata value to the node map table using the node pointer as the key
	lua_pushvalue(L, -1);
	lua_rawsetp(L, -3, node);
	lua_replace(L, -2);
}

/**
 * Returns the root element of the given HTML document
 *
 * @param lua_State *L Lua stack
 *
 * @return 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML document object
 *
 * Output:
 *   - userdata|nothing Node object or nil if not found
 *
 */
static int get_doc_element(lua_State *L)
{
	struct html_doc *hdoc = luaL_checkudata(L, 1, HTML_DOC_META_NAME);

	xmlNodePtr node = xmlDocGetRootElement(hdoc->doc);
	if (node == NULL) return 0;

	html_get_node_by_ptr(L, 1, node);

	return 1;
}

/**
 * Performs a XPath query and pushes the result onto the stack
 *
 * @param lua_State *L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML document object
 *   - [2] string   XPath query
 *
 * Output:
 *   - userdata with XPath result object
 */
static int eval_doc_xpath(lua_State *L)
{
	struct html_doc *hdoc = luaL_checkudata(L, 1, HTML_DOC_META_NAME);
	const char *query = luaL_checkstring(L, 2);

	xpath_create(L, 1); // [3] XPath result object

	xmlXPathContextPtr ctx = xmlXPathNewContext(hdoc->doc);
	if (ctx == NULL) {
		throw_parser_error(L, "Failed to make a parser context");
	}

	xmlXPathObjectPtr result = xmlXPathEval((const xmlChar *)query, ctx);
	xmlXPathFreeContext(ctx);
	if (result == NULL) {
		throw_parser_error(L, "Cannot eval XPath query");
	}

	xpath_set_result(L, 3, result);
	return 1;
}

/**
 * Frees resources of the HTML document structure
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML document object
 *
 * Output:
 *   nothing
 */
static int document_cleanup(lua_State *L)
{
	struct html_doc *hdoc = luaL_testudata(L, 1, HTML_DOC_META_NAME);
	if (hdoc) {
		xmlFreeDoc(hdoc->doc);
		hdoc->doc = NULL;
	}
	return 0;
}

/**
 * Implementation of the __index method for HTML document userdata
 *
 * @param lua_State *L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML document object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|nil
 */
static int get_htmldoc_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "documentElement", get_doc_element },
		{ "evalXPath", eval_doc_xpath },
		{ NULL, NULL }
	};

	luaL_checkudata(L, 1, HTML_DOC_META_NAME);

	if (lua_type(L, 2) == LUA_TSTRING) {
		if (get_cfunction(L, lua_tostring(L, 2), flist)) {
			return 1;
		}
	}

	lua_pushnil(L);
	return 1;
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
 *   - userdata HTML module object
 *
 * Output:
 *   nothing
 */
static void ensure_structures(lua_State *L)
{
	struct html_data *hdata = lua_touserdata(L, -1);
	if (!hdata->active) {
		hdata->active = true;
		if (module_started++) return;

		if (luaL_newmetatable(L, HTML_DOC_META_NAME)) {
			lua_pushcfunction(L, document_cleanup);
			lua_setfield(L, -2, "__gc");
			lua_pushcfunction(L, get_htmldoc_property);
			lua_setfield(L, -2, "__index");
		}
		if (luaL_newmetatable(L, HTML_NMAP_META_NAME)) {
			lua_pushliteral(L, "v");
			lua_setfield(L, -2, "__mode");
		}
		lua_pop(L, 2);

		xpath_init_structures(L);
		node_init_structures(L);
	}
}

/**
 * Loads HTML document from a char buffer
 *
 * @param lua_State  *L        Lua stack
 * @param const char *str      HTML string
 * @param size_t     len       HTML string length
 * @param const char *encoding Character encoding or NULL
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata HTML module object
 *
 * Output:
 *   - HTML document
 */
static void get_document(lua_State *L, const char *str, size_t len, const char *encoding)
{
	if (len > (size_t)INT_MAX) {
		lua_pushliteral(L, "HTML document is too big");
		lua_error(L);
	}

	ensure_structures(L);

	struct html_doc *hdoc = lua_newuserdata(L, sizeof(struct html_doc));
	hdoc->doc = NULL;
	luaL_getmetatable(L, HTML_DOC_META_NAME);
	lua_setmetatable(L, -2);

	// User value
	lua_newtable(L);

	// The module object
	lua_pushvalue(L, -3);
	lua_setfield(L, -2, "module");

	// Node map table
	lua_newtable(L);
	luaL_getmetatable(L, HTML_NMAP_META_NAME);
	lua_setmetatable(L, -2);
	lua_setfield(L, -2, "nodes");

	lua_setuservalue(L, -2);

	hdoc->doc = htmlReadMemory(
		str,
		(int)len,
		NULL,
		encoding,
		HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING
	);
	if (hdoc->doc == NULL) {
		throw_parser_error(L, "Failed to load html document");
	}
}

/**
 * Creates a new HTML module and loads HTML document from a char buffer
 *
 * @param lua_State  *L        Lua stack
 * @param const char *str      HTML string
 * @param size_t     len       HTML string length
 * @param const char *encoding Character encoding or NULL
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - HTML document
 */
void html_get_document(lua_State *L, const char *str, size_t len, const char *encoding)
{
	html_init(L);

	get_document(L, str, len, encoding);
	lua_replace(L, -2);
}

/**
 * Returns the raw pointer to the libxml2 document
 *
 * @param lua_State* L Lua stack
 *
 * @return htmlDocPtr
 *
 * === Lua stack ===
 *
 * Input:
 *   - HTML document object
 *
 * Output:
 *   nothing
 */
htmlDocPtr html_get_document_ptr(lua_State *L)
{
	const struct html_doc *hdoc = lua_touserdata(L, -1);
	return hdoc->doc;
}

/**
 * Loads HTML document from a Lua string
 *
 * @param lua_State *L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata   HTML module object
 *   - [2] string     HTML document presented as a string
 *   - [3] string|nil HTML document encoding
 *
 * Output:
 *   - userdata with HTML document
 */
static int load_from_string(lua_State *L)
{
	lua_settop(L, 3);
	luaL_checkudata(L, 1, HTML_MODULE_META_NAME);
	int arg3type = lua_type(L, 3);
	luaL_argcheck(L, (arg3type == LUA_TSTRING || arg3type == LUA_TNIL), 3, "string or nil expected");

	size_t str_len;
	const char *str = luaL_checklstring(L, 2, &str_len);
	const char *enc = arg3type == LUA_TSTRING ? lua_tostring(L, 3) : NULL;

	lua_rotate(L, 1, -1);
	get_document(L, str, str_len, enc);

	return 1;
}

/**
 * Implementation of the __index method for the module userdata
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML module object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|nil
 */
static int get_module_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "fromString", load_from_string },
		{ NULL, NULL }
	};

	luaL_checkudata(L, 1, HTML_MODULE_META_NAME);

	if (lua_type(L, 2) == LUA_TSTRING) {
		if (get_cfunction(L, lua_tostring(L, 2), flist)) {
			return 1;
		}
	}

	lua_pushnil(L);
	return 1;
}

/**
 * Frees the module's service structures
 *
 * @param lua_State *L Lua stack
 *
 * return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML module object
 *
 * Output:
 *   nothing
 */
static int html_unload(lua_State *L)
{
	const struct html_data *hdata = luaL_testudata(L, 1, HTML_MODULE_META_NAME);
	if (hdata && hdata->active) {
		if (!--module_started) {
			xmlCleanupParser();
		}
	}
	return 0;
}

/**
 * Creates and initiates the HTML module
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
 *   - userdata HTML module object
 */
void html_init(lua_State *L)
{
	struct html_data *hdata = lua_newuserdata(L, sizeof(struct html_data));
	hdata->active = false;

	if (luaL_newmetatable(L, HTML_MODULE_META_NAME)) {
		lua_pushcfunction(L, html_unload);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, get_module_property);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
}
