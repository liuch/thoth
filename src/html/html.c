#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <libxml2/libxml/HTMLparser.h>
#include <libxml2/libxml/xpath.h>
#include <libxml2/libxml/xmlerror.h>
#include "../lua.h"
#include "html.h"

#define HTML_MODULE_META_NAME "thoth.html"
#define HTML_DOC_META_NAME    "thoth.html.doc"
#define HTML_XPATH_META_NAME  "thoth.html.xpath"
#define HTML_NODE_META_NAME   "thoth.html.node"
#define HTML_NMAP_META_NAME   "thoth.html.node.map"

struct html_data {
	bool active;
};

struct html_doc {
	htmlDocPtr doc;
};

struct xpath_data {
	xmlXPathObjectPtr obj;
};

struct node_data {
	xmlNodePtr node;
};

static int module_started = 0;

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
static void throw_parser_error(lua_State *L, const char *err_string)
{
	const xmlError *error = xmlGetLastError();
	lua_pushstring(L, error != NULL ? error->message : err_string);
	lua_error(L);
}

/**
 * Creates and return a new lua user data object with the node data in it
 *
 * @param lua_State         *L        Lua stack
 * @param int               doc_index Absolute stack index where the HTML document is
 * @param struct xmlNodePtr node      Node to create the object for
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
static void get_node_udata(lua_State *L, int doc_index, xmlNodePtr node)
{
	// Get node map table
	lua_getuservalue(L, doc_index);
	lua_getfield(L, -1, "nodes");
	lua_replace(L, -2);

	// If a lua object has already been created for this node, return it.
	if (lua_rawgetp(L, -1, node) != LUA_TNIL) {
		lua_replace(L, -2);
		return;
	}
	lua_pop(L, 1);

	// Make a new node userdata
	struct node_data *ndata = lua_newuserdata(L, sizeof(struct node_data));
	ndata->node = NULL;
	luaL_getmetatable(L, HTML_NODE_META_NAME);
	lua_setmetatable(L, -2);
	ndata->node = node;

	// Bind the node to the HTML root element
	// This is necessary to prevent the HTML object from being freed by the garbage collector
	// while the node is still in use.
	lua_pushvalue(L, doc_index);
	lua_setuservalue(L, -2);

	// Set the udata value to the node map table using the node pointer as the key
	lua_pushvalue(L, -1);
	lua_rawsetp(L, -3, node);
	lua_replace(L, -2);
}

/**
 * Makes a table and fills it with node objects from the nodeset
 *
 * @param lua_State     *L        Lua stack
 * @param int           doc_index Absolute stack index where the HTML document is
 * @param xmlNodeSetPtr nodeset   Set of nodes from libxml2
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
static void make_nodeset_table(lua_State *L, int doc_index, xmlNodeSetPtr nodeset)
{
	lua_newtable(L);
	if (nodeset == NULL) return;

	for (int i = 0; i < nodeset->nodeNr; ) {
		get_node_udata(L, doc_index, nodeset->nodeTab[i]);
		lua_rawseti(L, -2, ++i);
	}
}

/**
 * Makes an empty XPath result object and pushes it onto the stack
 *
 * @param lua_State     *L        Lua stack
 * @param int           doc_index Absolute stack index where the HTML document is
 *
 * @return struct xpath_data Pointer to the new C structure
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - userdata Empty XPath result object
 */
static struct xpath_data *make_xpath_data(lua_State *L, int doc_index)
{
	struct xpath_data *xdata = lua_newuserdata(L, sizeof(struct xpath_data));
	xdata->obj = NULL;
	luaL_getmetatable(L, HTML_XPATH_META_NAME);
	lua_setmetatable(L, -2);

	// Bind the XPath result data to the HTML root element
	// This is necessary to prevent the HTML object from being freed by the garbage collector
	// while the XPath result object is still in use.
	lua_pushvalue(L, doc_index);
	lua_setuservalue(L, -2);

	return xdata;
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

	get_node_udata(L, 1, node);

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
 *   - userdata with XPath result data
 */
static int eval_doc_xpath(lua_State *L)
{
	struct html_doc *hdoc = luaL_checkudata(L, 1, HTML_DOC_META_NAME);
	const char *query = luaL_checkstring(L, 2);

	struct xpath_data *xdata = make_xpath_data(L, 1);

	xmlXPathContextPtr ctx = xmlXPathNewContext(hdoc->doc);
	if (ctx == NULL) {
		throw_parser_error(L, "Failed to make a parser context");
	}

	xdata->obj = xmlXPathEval((const xmlChar *)query, ctx);
	xmlXPathFreeContext(ctx);
	if (xdata->obj == NULL) {
		throw_parser_error(L, "Cannot eval XPath query");
	}

	return 1;
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
static int get_xpath_result_type(lua_State *L)
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
 static int get_xpath_result_size(lua_State *L)
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
static int get_xpath_result_value(lua_State *L)
{
	struct xpath_data *xdata = luaL_checkudata(L, 1, HTML_XPATH_META_NAME);
	assert(xdata->obj != NULL);

	xmlNodeSetPtr nodeset;

	switch (xdata->obj->type) {
		case XPATH_NODESET:
			int res_stack_index = lua_isnoneornil(L, 2) ? 0 : 2;

			lua_getuservalue(L, 1);
			int doc_stack_index = lua_absindex(L, -1);

			nodeset = xdata->obj->nodesetval;
			if (res_stack_index == 0) {
				make_nodeset_table(L, doc_stack_index, nodeset);
				break;
			} else if (lua_isinteger(L, res_stack_index)) {
				if (nodeset != NULL) {
					int num = lua_tointeger(L, 2);
					if (num > 0 && num <= xdata->obj->nodesetval->nodeNr) {
						get_node_udata(L, doc_stack_index, nodeset->nodeTab[num - 1]);
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
 * Returns the attribute value of the node object
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Node object
 *   - [2] string   Attribute name
 *
 * Output:
 *   - string|nil Attribute value or nil if the node have no the specified attribute
 */
static int get_node_attribute(lua_State *L)
{
	struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	const char *name = luaL_checkstring(L, 2);

	if (ndata->node->type != XML_ELEMENT_NODE) return 0;

	xmlChar *val = xmlGetProp(ndata->node, (const xmlChar *)name);
	if (val == NULL) return 0;

	lua_pushstring(L, (const char *)val);
	xmlFree(val);
	return 1;
}

/**
 * Returns a key-value list of the node attributes or nil if the node is not an element
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Node object
 *
 * Output:
 *   - table|nil
 */
static int get_node_attributes(lua_State *L)
{
	struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	if (ndata->node->type != XML_ELEMENT_NODE) return 0;

	lua_newtable(L);
	xmlAttrPtr attr = ndata->node->properties;
	for ( ; attr; attr = attr->next) {
		lua_pushstring(L, (const char *)attr->name);
		xmlChar *value = xmlNodeGetContent(attr->children);
		lua_pushstring(L, (const char *)value);
		xmlFree(value);
		lua_rawset(L, -3);
	}
	return 1;
}

/**
 * Returns the type of the node object
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack
 *
 * Input:
 *   - [1] userdata Node object
 *
 * Output:
 *   - string Node type as a string or nil if the type is unknown
 */
static int get_node_type(lua_State *L)
{
	struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	switch (ndata->node->type) {
		case XML_ELEMENT_NODE:
		case XML_DOCUMENT_NODE:
		case XML_HTML_DOCUMENT_NODE:
			lua_pushliteral(L, "element");
			break;
		case XML_TEXT_NODE:
			lua_pushliteral(L, "text");
			break;
		case XML_ATTRIBUTE_NODE:
			lua_pushliteral(L, "attribute");
			break;
		case XML_CDATA_SECTION_NODE:
			lua_pushliteral(L, "cdata");
			break;
		case XML_COMMENT_NODE:
			lua_pushliteral(L, "comment");
			break;
		default:
			return 0;
	}
	return 1;
}

/**
 * Returns the outer content of the node object as HTML text
 *
 * @param lua_State *L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Node object
 *
 * Output:
 *   - string|nil
 */
static int get_outer_html(lua_State *L)
{
	struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	xmlBufferPtr buffer = xmlBufferCreate();
	if (buffer == NULL)
		throw_out_of_memory(L, "html");

	lua_getuservalue(L, 1);
	struct html_doc *hdoc = lua_touserdata(L, -1);

	int bytes = xmlNodeDump(buffer, hdoc->doc, ndata->node, 0, 0);
	if (bytes >= 0) {
		lua_pushlstring(L, (const char *)xmlBufferContent(buffer), (size_t)bytes);
	} else {
		lua_pushnil(L);
	}
	xmlBufferFree(buffer);
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
 *   - [1] userdata Node object
 *   - [2] string   XPath query
 *
 * Output:
 *   - userdata XPath result object
 */
static int eval_node_xpath(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	const char *query = luaL_checkstring(L, 2);

	lua_getuservalue(L, 1);
	struct html_doc *hdoc = lua_touserdata(L, -1);
	struct xpath_data *xdata = make_xpath_data(L, lua_absindex(L, -1));
	xmlXPathContextPtr ctx = xmlXPathNewContext(hdoc->doc);
	lua_replace(L, -2);

	if (ctx == NULL) {
		throw_parser_error(L, "Failed to make a parser context");
	}
	ctx->node = ndata->node;

	xdata->obj = xmlXPathEval((const xmlChar *)query, ctx);
	xmlXPathFreeContext(ctx);
	if (xdata->obj == NULL) {
		throw_parser_error(L, "Cannot eval XPath query");
	}

	return 1;
}

/**
 * Returns the node name
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack
 *
 * Input:
 *   - [1] userdata Node object
 *
 * Output:
 *   - string|nil
 */
static int get_node_name(lua_State *L)
{
	struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	if (ndata->node->name == NULL) return 0;
	lua_pushstring(L, (const char *)ndata->node->name);
	return 1;
}

/**
 * Returns the content of the node object
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack
 *
 * Input:
 *   - [1] userdata Node object
 *
 * Output:
 *   - string|nil
 */
static int get_node_content(lua_State *L)
{
	struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	xmlChar *text;
	switch (ndata->node->type) {
		case XML_ELEMENT_NODE:
		case XML_TEXT_NODE:
		case XML_ATTRIBUTE_NODE:
		case XML_COMMENT_NODE:
			text = xmlNodeGetContent(ndata->node);
			if (text == NULL)
				throw_out_of_memory(L, "html");
			lua_pushstring(L, (const char *)text);
			xmlFree(text);
			break;
		default:
			return 0;
	}
	return 1;
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
static int xpath_cleanup(lua_State *L)
{
	struct xpath_data *xdata = luaL_checkudata(L, 1, HTML_XPATH_META_NAME);
	xmlXPathFreeObject(xdata->obj);
	xdata->obj = NULL;
	return 0;
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
	struct html_doc *hdoc = luaL_checkudata(L, 1, HTML_DOC_META_NAME);
	xmlFreeDoc(hdoc->doc);
	hdoc->doc = NULL;
	return 0;
}

/**
 * Implementation of the __index method for HTML document userdata
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTML document object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction
 */
static int get_htmldoc_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "documentElement", get_doc_element },
		{ "evalXPath", eval_doc_xpath },
		{ NULL, NULL }
	};

	const char *pname = luaL_checkstring(L, 2);
	return get_cfunction(L, pname, flist);
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
static int get_htmlxpath_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "type", get_xpath_result_type },
		{ "size", get_xpath_result_size },
		{ "result", get_xpath_result_value },
		{ NULL, NULL }
	};

	const char *pname = luaL_checkstring(L, 2);
	return get_cfunction(L, pname, flist);
}

/**
 * Implementation of the __index method for HTML node userdata
 *
 * @param lua_State *L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Node object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction
 */
static int get_htmlnode_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "attributes", get_node_attributes },
		{ "attribute", get_node_attribute },
		{ "name", get_node_name },
		{ "type", get_node_type },
		{ "textContent", get_node_content },
		{ "outerHTML", get_outer_html },
		{ "evalXPath", eval_node_xpath },
		{ NULL, NULL }
	};

	const char *pname = luaL_checkstring(L, 2);
	return get_cfunction(L, pname, flist);
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
		if (luaL_newmetatable(L, HTML_XPATH_META_NAME)) {
			lua_pushcfunction(L, xpath_cleanup);
			lua_setfield(L, -2, "__gc");
			lua_pushcfunction(L, get_htmlxpath_property);
			lua_setfield(L, -2, "__index");
		}
		if (luaL_newmetatable(L, HTML_NODE_META_NAME)) {
			lua_pushcfunction(L, get_htmlnode_property);
			lua_setfield(L, -2, "__index");
		}
		if (luaL_newmetatable(L, HTML_NMAP_META_NAME)) {
			lua_pushliteral(L, "v");
			lua_setfield(L, -2, "__mode");
		}

		lua_pop(L, 4);
	}
}

/**
 * Loads HTML document from a char buffer
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata HTML module object
 *
 * Output:
 *   - html document
 */
static void get_document(lua_State *L, const char *str, size_t len)
{
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

	hdoc->doc = htmlReadMemory(str, len, NULL, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	if (hdoc->doc == NULL) {
		throw_parser_error(L, "Failed to load html document");
	}
}

/**
 * Creates a new HTML module and loads HTML document from a char buffer
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - HTML document
 */
void html_getdocument(lua_State *L, const char *str, size_t len)
{
	html_init(L);

	get_document(L, str, len);
	lua_replace(L, -2);
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
 *   - [1] userdata HTML module object
 *   - [2] string   HTML document presented as a string
 *
 * Output:
 *   - userdata with HTML document
 */
static int load_from_string(lua_State *L)
{
	luaL_checkudata(L, 1, HTML_MODULE_META_NAME);
	const char *str = luaL_checkstring(L, 2);

	lua_settop(L, 2);
	lua_insert(L, 1);
	get_document(L, str, luaL_len(L, 1));

	return 1;
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

	return get_cfunction(L, luaL_checkstring(L, 2), flist);
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
	const struct html_data *hdata = lua_touserdata(L, 1);
	if (hdata->active) {
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
