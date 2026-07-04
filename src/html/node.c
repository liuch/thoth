#include <libxml2/libxml/xpath.h>
#include "node.h"
#include "html.h"
#include "xpath.h"
#include "error.h"

#define HTML_NODE_META_NAME   "thoth.html.node"

struct node_data {
	xmlNodePtr node;
};

/**
 * Creates and returns a new lua user data object with the node data in it
 *
 * @param lua_State* L            Lua stack
 * @param int        document_pos HTML document position
 * @param xmlNodePtr node         Node to create the object for
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
void node_create(lua_State *L, int document_pos, xmlNodePtr node)
{
	// Make a new node userdata
	struct node_data *ndata = lua_newuserdata(L, sizeof(struct node_data));
	ndata->node = NULL;
	luaL_getmetatable(L, HTML_NODE_META_NAME);
	lua_setmetatable(L, -2);
	ndata->node = node;

	// Bind the node to the HTML document
	// This is necessary to prevent the HTML object from being freed by the garbage collector
	// while the node is still in use.
	lua_pushvalue(L, document_pos);
	lua_setuservalue(L, -2);
}

/**
 * Returns the attribute value of the node object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
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
static int get_attribute(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
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
 * @param lua_State* L Lua stack
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
static int get_attributes(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
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
static int get_type(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
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
static int get_name(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
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
static int get_content(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
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
	htmlDocPtr doc = html_get_document_ptr(L);

	int bytes = xmlNodeDump(buffer, doc, ndata->node, 0, 0);
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
static int eval_xpath(lua_State *L)
{
	const struct node_data *ndata = luaL_checkudata(L, 1, HTML_NODE_META_NAME);
	const char *query = luaL_checkstring(L, 2);

	lua_getuservalue(L, 1); // [3] HTML document object
	htmlDocPtr doc = html_get_document_ptr(L);

	xpath_create(L, 3);
	lua_replace(L, -2); // [3] XPath result object

	xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
	if (ctx == NULL) {
		throw_parser_error(L, "Failed to make a parser context");
	}
	ctx->node = ndata->node;

	xmlXPathObjectPtr result = xmlXPathEval((const xmlChar *)query, ctx);
	xmlXPathFreeContext(ctx);
	if (result == NULL) {
		throw_parser_error(L, "Cannot eval XPath query");
	}

	xpath_set_result(L, 3, result);
	return 1;
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
static int get_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "attributes", get_attributes },
		{ "attribute", get_attribute },
		{ "name", get_name },
		{ "type", get_type },
		{ "textContent", get_content },
		{ "outerHTML", get_outer_html },
		{ "evalXPath", eval_xpath },
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
void node_init_structures(lua_State *L)
{
	if (luaL_newmetatable(L, HTML_NODE_META_NAME)) {
		lua_pushcfunction(L, get_property);
		lua_setfield(L, -2, "__index");
	}
	lua_pop(L, 1);
}
