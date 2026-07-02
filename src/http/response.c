#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>
#include "../lua.h"
#include "../json/json.h"
#include "../html/html.h"
#include "response.h"

#define HTTP_RESP_META_NAME "thoth.http.response"

/**
 * Frees resources of the http response object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP response object
 *
 * Output:
 *   noting
 */
static int cleanup_response(lua_State *L)
{
	struct http_resp *resp = luaL_checkudata(L, 1, HTTP_RESP_META_NAME);
	free_buffer(&resp->buffer);
	luaL_unref(L, LUA_REGISTRYINDEX, resp->headers_ref);
	luaL_unref(L, LUA_REGISTRYINDEX, resp->cookies_ref);
	resp->headers_ref = LUA_REFNIL;
	resp->cookies_ref = LUA_REFNIL;
	return 0;
}

/**
 * Returns the HTTP response body as a Lua string
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP response object
 *
 * Output:
 *   - string Body as a string
 */
static int get_response_text(lua_State *L)
{
	struct http_resp *resp = luaL_checkudata(L, 1, HTTP_RESP_META_NAME);
	if (resp->buffer.pointer != NULL && resp->buffer.size != 0) {
		lua_pushlstring(L, resp->buffer.pointer, resp->buffer.size);
		free_buffer(&resp->buffer);
	} else {
		lua_pushliteral(L, "");
	}

	return 1;
}

/**
 * Returns the JSON body of the HTTP response as a Lua value.
 * It returns nothing when the response has no data.
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP response object
 *
 * Output:
 *   - table|boolean|string|number|nothing
 */
static int get_response_json(lua_State *L)
{
	struct http_resp *resp = luaL_checkudata(L, 1, HTTP_RESP_META_NAME);
	if (resp->buffer.pointer != NULL && resp->buffer.size != 0) {
		json_decode_string(L, resp->buffer.pointer, resp->buffer.size);
		free_buffer(&resp->buffer);
		return 1;
	}
	return 0;
}

/**
 * Returns the HTML body of the HTTP response object.
 * It returns nothing when the response has not data.
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
 *
 * Input:
 *   - [1] userdata HTTP response object
 *
 * Output:
 *   - userdata|nothing HTML document object
 */
static int get_response_html(lua_State *L)
{
	struct http_resp *resp = luaL_checkudata(L, 1, HTTP_RESP_META_NAME);
	if (resp->buffer.pointer != NULL && resp->buffer.size != 0) {
		html_getdocument(L, (const char *)resp->buffer.pointer, resp->buffer.size);
		free_buffer(&resp->buffer);
		return 1;
	}
	return 0;
}

/**
 * Returns the HTTP headers from the HTTP response object
 *
 * When the second parameter is specified the function returns
 * the value of the header with the passed name as a Lua string.
 * Otherwise, the function returns a Lua key-value table with the headers
 * or nothing if the response has no headers.
 *
 * @param lua_State* L
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP response object
 *   - [2] string   Header name. Optional
 *
 * Output:
 *   - table|string|nil|nothing List of the headers or the header value with the passed name
 */
static int get_response_headers(lua_State *L)
{
	int args = lua_gettop(L);
	if (args < 1 || args > 2) {
		lua_pushliteral(L, "The function expects one or two parameters");
		lua_error(L);
	}

	struct http_resp *resp = luaL_checkudata(L, 1, HTTP_RESP_META_NAME);
	int h_type = lua_rawgeti(L, LUA_REGISTRYINDEX, resp->headers_ref);
	if (args == 1 || h_type != LUA_TTABLE) return 1;

	luaL_argcheck(L, lua_type(L, 2) == LUA_TSTRING, 2, "string expected");
	char *name = my_strdup(lua_tostring(L, 2));
	if (name == NULL) {
		throw_out_of_memory(L, "get_response_headers");
	}
	for (char *p = name; *p; ++p) *p = tolower(*p);

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (lua_type(L, -2) == LUA_TSTRING) {
			if (strcmp(name, lua_tostring(L, -2)) == 0) {
				free(name);
				return 1;
			}
		}
		lua_pop(L, 1);
	}
	free(name);
	return 0;
}

/**
 * Returns the cookies from the HTTP response object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata HTTP response object
 *
 * Output:
 *   - table|nil
 */
static int get_response_cookies(lua_State *L)
{
	if (lua_gettop(L) != 1) {
		lua_pushliteral(L, "The function expects one parameter");
		lua_error(L);
	}

	struct http_resp *resp = luaL_checkudata(L, 1, HTTP_RESP_META_NAME);
	lua_rawgeti(L, LUA_REGISTRYINDEX, resp->cookies_ref);
	return 1;
}

/**
 * Implementation of the __index method for the HTTP response userdata
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ====
 *
 * Input:
 *   - [1] userdata HTTP response object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|bool|number|string|nothing
 */
static int get_response_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "text", get_response_text },
		{ "json", get_response_json },
		{ "html", get_response_html },
		{ "headers", get_response_headers },
		{ "cookies", get_response_cookies },
		{ NULL, NULL }
	};

	struct http_resp *resp = lua_touserdata(L, 1);
	const char *pname = luaL_checkstring(L, 2);
	if (!get_cfunction(L, pname, flist)) {
		if (strcmp(pname, "ok") == 0) {
			lua_pushboolean(L, resp->ok);
		} else if (strcmp(pname, "status") == 0) {
			lua_pushinteger(L, resp->status);
		} else if (strcmp(pname, "error") == 0) {
			if (resp->error[0] == '\0') {
				return 0;
			}
			lua_pushstring(L, resp->error);
		} else {
			return 0;
		}
	}
	return 1;
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
 *   nothing
 *
 * Output:
 *   nothing
 */
void response_init_structures(lua_State *L)
{
	luaL_newmetatable(L, HTTP_RESP_META_NAME);
	lua_pushcfunction(L, cleanup_response);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, get_response_property);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

/**
 * Creates an empty response object
 *
 * @param lua_State* L Lua stack
 *
 * @return a pointer to the HTTP response structure
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - userdata HTTP response object
 */
struct http_resp *response_create(lua_State *L)
{
	struct http_resp *resp = lua_newuserdata(L, sizeof(struct http_resp));
	resp->ok = false;
	resp->status = 0;
	resp->error[0] = '\0';
	resp->buffer.pointer = NULL;
	resp->buffer.size = 0;
	resp->headers_ref = LUA_REFNIL;
	resp->cookies_ref = LUA_REFNIL;
	luaL_getmetatable(L, HTTP_RESP_META_NAME);
	lua_setmetatable(L, -2);
	return resp;
}

/**
 * Extracts the HTTP headers from the cURL socket and stores them into the HTTP response structure
 *
 * @param lua_State*        L      Lua stack
 * @param struct http_resp* resp   HTTP response structure
 * @param CURL*             handle cURL socket handle
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
void response_saveheaders(lua_State *L, struct http_resp *resp, CURL *handle)
{
	assert(resp != NULL);
	assert(handle != NULL);
	assert(resp->headers_ref == LUA_REFNIL);

	lua_newtable(L);
	struct curl_header *h;
	struct curl_header *prev = NULL;
	while ((h = curl_easy_nextheader(handle, CURLH_HEADER, -1, prev))) {
		char *name = my_strdup(h->name);
		if (name == NULL) {
			throw_out_of_memory(L, "get_response_headers");
		}
		for (char *p = name; *p; ++p) *p = tolower(*p);
		lua_pushstring(L, name);
		lua_pushstring(L, h->value);
		lua_settable(L, -3);
		free(name);
		prev = h;
	}
	resp->headers_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

/**
 * Extracts the cookies from the cURL socket and stores them into the HTTP response structure
 *
 * @param lua_State*        L      Lua stack
 * @param struct http_resp* resp   HTTP response structure
 * @param CURL*             handle cURL socket handle
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
void response_savecookies(lua_State *L, struct http_resp *resp, CURL *handle)
{
	assert(resp != NULL);
	assert(handle != NULL);
	assert(resp->cookies_ref == LUA_REFNIL);

	struct curl_slist *cookies = NULL;
	if (curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) {
		lua_pushliteral(L, "curl_easy_getinfo failed");
		lua_error(L);
	}

	lua_newtable(L);

	if (cookies != NULL) {
		int i = 0;
		struct curl_slist *it = cookies;
		do {
			lua_pushstring(L, it->data);
			lua_rawseti(L, -2, ++i);
			it = it->next;
		} while (it != NULL);
		curl_slist_free_all(cookies);
	}

	resp->cookies_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}
