#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <assert.h>
#include "socket.h"
#include "dataset.h"
#include "libcurl.h"
#include "response.h"
#include "../common.h"
#include "../version.h"

#define HTTP_SOCKET_META_NAME  "thoth.socket"

struct curl_data {
	CURL   *handle;
	char   error[CURL_ERROR_SIZE];
	struct buffer writebuffer;
	struct curl_slist *user_headers;
	int    queries;
	bool   custom_useragent;
	bool   new_cookiejar;
	struct buffer    fields_data;
	struct curl_mime *multipart_data;
};

/**
 * Throws an error related to setting connection options
 *
 * @param lua_State*  L           Lua stack
 * @param int         err_code    Error code
 * @param const char* err_message Error message
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
static void throw_setopt_error(lua_State *L, int err_code, const char *err_message)
{
	lua_pushfstring(L, "setopt: %s (%d)\n", err_message, err_code);
	lua_error(L);
}

/**
 * Returns a string message of the last error
 *
 * @param CURLcode          error_num Error code
 * @param struct curl_data* cdata     Socket data structure
 *
 * @return const char*
 */
static const char* get_error_string(CURLcode error_num, const struct curl_data *cdata)
{
	if (cdata->error[0] != '\0') {
		return cdata->error;
	}
	return curl_easy_strerror(error_num);
}

/**
 * Check if the given parameter is a curl_data structure and socket is not closed
 *
 * @param lua_State* L           Lua stack
 * @param int        param_index The stack position the socket object is at
 *
 * @return struct curl_data* Socket data structure
 *
 * === Lua ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static struct curl_data *check_curl_data_param(lua_State *L, int param_index)
{
	struct curl_data *cdata = luaL_checkudata(L, param_index, HTTP_SOCKET_META_NAME);
	if (cdata->handle == NULL) {
		lua_pushliteral(L, "The socket is closed");
		lua_error(L);
	}
	return cdata;
}

/**
 * This callback function gets called by libcurl as soon as there is data received that needs to be saved
 *
 * @param char*  data   Pointer to the delivered data
 * @param size_t size   Number of data blocks
 * @param size_t nmemb  Size of the data block
 * @param void*  udata  User data. Here it is the socket data structure
 *
 * @return size_t Number of bytes saved
 */
static size_t writefunction(const char *data, size_t size, size_t nmemb, void *udata)
{
	struct curl_data *ud = udata;
	size_t sz = size * nmemb;

	char *ptr = realloc(ud->writebuffer.pointer, ud->writebuffer.size + sz);
	if (ptr == NULL) return 0;

	memcpy(ptr + ud->writebuffer.size, data, sz);
	ud->writebuffer.pointer = ptr;
	ud->writebuffer.size += sz;

	return sz;
}

/**
 * Makes an empty userdata to control a curl easy socket
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   [1] userdata HTTP module
 *
 * Output:
 *   - userdata Socket object
 */
void socket_create(lua_State *L)
{
	struct curl_data *cdata = lua_newuserdata(L, sizeof(struct curl_data));
	cdata->handle = NULL;
	cdata->error[0] = '\0';
	cdata->writebuffer.pointer = NULL;
	cdata->writebuffer.size = 0;
	cdata->user_headers = NULL;
	cdata->queries = 0;
	cdata->custom_useragent = false;
	cdata->new_cookiejar = false;
	cdata->fields_data.pointer = NULL;
	cdata->fields_data.size = 0;
	cdata->multipart_data = NULL;

	luaL_getmetatable(L, HTTP_SOCKET_META_NAME);
	lua_setmetatable(L, -2);

	// Init curl socket
	cdata->handle = curl_easy_init();
	if (cdata->handle == NULL) {
		lua_pushliteral(L, "curl_easy_init failed");
		lua_error(L);
	}
	// Set curl error buffer
	CURLcode res = curl_easy_setopt(cdata->handle, CURLOPT_ERRORBUFFER, cdata->error);
	if (res != CURLE_OK) {
		throw_setopt_error(L, res, get_error_string(res, cdata));
	}

	// Socket userdata table
	lua_newtable(L);
	dataset_init(L);
	// Set socket userdata
	lua_setuservalue(L, -2);
}

/**
 * Frees the memory of the request's outgoing data
 *
 * @param struct curl_data* cdata Socket data structure
 *
 * @return void
 */
static void free_request_outgoing_data(struct curl_data *cdata)
{
	free_buffer(&cdata->fields_data);
	curl_mime_free(cdata->multipart_data);
	cdata->multipart_data = NULL;
	curl_slist_free_all(cdata->user_headers);
	cdata->user_headers = NULL;
}

/**
 * Sets socket options
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata   Socket object
 *   - [2] string     Option name.
 *                    The valid values are "proxy", "useragent", "proxyusername", "proxypassword"
 *   - [3] string|nil Option value
 *
 * Output:
 *   - userdata Self
 */
static int set_opt(lua_State *L)
{
	struct curl_data *cdata = check_curl_data_param(L, 1);
	const char *opt = luaL_checkstring(L, 2);
	assert(cdata->handle != NULL);

	CURLoption opt_num;
	bool custom_ua = false;
	if (strcmp(opt, "proxy") == 0) {
		opt_num = CURLOPT_PROXY;
	} else if (strcmp(opt, "proxyusername") == 0) {
		opt_num = CURLOPT_PROXYUSERNAME;
	} else if (strcmp(opt, "proxypassword") == 0) {
		opt_num = CURLOPT_PROXYPASSWORD;
	} else if (strcmp(opt, "useragent") == 0) {
		opt_num = CURLOPT_USERAGENT;
		custom_ua = true;
	} else {
		lua_pushfstring(L, "Unknown socket option: %s", opt);
		lua_error(L);
	}

	const char *val = lua_isnoneornil(L, 3) ? NULL : luaL_checkstring(L, 3);
	CURLcode res = curl_easy_setopt(cdata->handle, opt_num, val);
	if (res != CURLE_OK) {
		throw_setopt_error(L, res, get_error_string(res, cdata));
	}

	if (custom_ua) {
		cdata->custom_useragent = true;
	}

	lua_pushvalue(L, 1);
	return 1;
}

/**
 * Sets a new cookie jar key for the given socket object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 2
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Socket object
 *   - [2] any      New cookie jar key. A nil value disables the cookie engine
 *   - [3] boolean  Type of context to store a cookie jar. True - for all sockets, false - for this one.
 *
 * Output:
 *   - any          Previous cookie jar key
 *   - boolean      Previous type of context
 */
static int set_cookies(lua_State *L)
{
	struct curl_data *cdata = check_curl_data_param(L, 1);

	switch (lua_gettop(L)) {
		case 1:
			lua_pushnil(L);
			// fallthrough
		case 2:
			lua_pushboolean(L, false);
			break;
		case 3:
			luaL_checktype(L, 3, LUA_TBOOLEAN);
			break;
		default:
			lua_pushliteral(L, "The function expects not more than three arguments");
			lua_error(L);
	}

	lua_getuservalue(L, 1);
	lua_rotate(L, -3, 1);
	dataset_set_new_cookiejar_params(L);
	lua_rotate(L, -3, -1);
	dataset_get_active_cookiejar_params(L);

	cdata->new_cookiejar = true;

	return 2;
}

/**
 * Iterates custom HTTP headers in a Lua table and stores them into the socket structure
 *
 * @param lua_State*        L       Lua stack
 * @param int               t_index Stack position the header table is at
 * @param struct curl_data* cdata   Socket data structure
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
static void set_user_headers(lua_State *L, int t_index, struct curl_data *cdata)
{
	char numb_str[64];
	struct curl_slist *h = NULL;
	int type = lua_getfield(L, t_index, "headers");
	if (type == LUA_TTABLE) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			if (lua_type(L, -2) == LUA_TSTRING) {
				const char *key = lua_tostring(L, -2);
				size_t klen = strlen(key);
				if (klen) {
					const char *val;
					const int vtype = lua_type(L, -1);
					switch (vtype) {
						case LUA_TSTRING:
							val = lua_tostring(L, -1);
							break;
						case LUA_TNUMBER:
							lua_pushvalue(L, -1);
							strcpy(numb_str, lua_tostring(L, -1));
							lua_pop(L, 1);
							val = numb_str;
							break;
						case LUA_TBOOLEAN:
							val = lua_toboolean(L, -1) ? "true" : "false";
							break;
						default:
							lua_pushfstring(
								L,
								"Incorrect type of the header value: %s",
								lua_typename(L, vtype)
							);
							lua_error(L);
					}
					size_t vlen = strlen(val);
					char *tmp = malloc(klen + vlen + 3);
					if (tmp == NULL) {
						throw_out_of_memory(L, "set_user_headers");
					}
					// cppcheck-suppress nullPointerOutOfMemory
					strcpy(tmp, key);
					if (vlen) {
						strcpy(tmp + klen, ": ");
						strcpy(tmp + klen + 2, val);
					} else {
						strcpy(tmp + klen, ":");
					}
					h = curl_slist_append(h, tmp);
					free(tmp);
				}
			}
			lua_pop(L, 1);
		}
	} else if (type != LUA_TNIL) {
		lua_pushliteral(L, "Invalid header list: table or nil expected");
		lua_error(L);
	}
	lua_pop(L, 1); // table 'headers' or nil

	cdata->user_headers = h;
}

/**
 * Generates a query data string from a Lua key-value table and stores it into the socket structure
 *
 * @param lua_State*        L          Lua stack
 * @param int               data_index Stack position the fields data is
 * @param struct curl_data* cdata      Socket data structure
 *
 * @return long Length of the query data string
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static long set_http_fields(lua_State *L, int data_index, struct curl_data *cdata)
{
	assert(cdata->fields_data.pointer == NULL);
	size_t size = 0;
	size_t len  = 0;
	char numb_str[64];
	lua_pushnil(L);
	char *p_fields = cdata->fields_data.pointer;
	while (lua_next(L, data_index) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING) continue;
		const char *k_str = lua_tostring(L, -2);
		int v_type = lua_type(L, -1);
		const char *v_str;
		size_t v_size;
		switch (v_type) {
			case LUA_TSTRING:
				v_str = lua_tostring(L, -1);
				v_size = luaL_len(L, -1);
				break;
			case LUA_TBOOLEAN:
				if (lua_toboolean(L, -1)) {
					v_str = "true";
					v_size = 4;
				} else {
					v_str = "false";
					v_size = 5;
				}
				break;
			case LUA_TNUMBER:
				lua_pushvalue(L, -1);
				strcpy(numb_str, lua_tostring(L, -1));
				lua_pop(L, 1);
				v_str = numb_str;
				v_size = 0;
				break;
			default:
				lua_pushfstring(L, "Incorrect type of the body value: %s [%s]", k_str, lua_typename(L, v_type));
				lua_error(L);
		}

		char *v_estr = curl_easy_escape(cdata->handle, v_str, v_size);
		if (v_estr == NULL) {
			throw_out_of_memory(L, "HTTP escape");
		}

		size_t item_size = strlen(k_str) + strlen(v_estr) + 2;
		if (len + item_size + 1 > size) {
			size = len + (item_size < 64 ? 64 : item_size + 1);
			p_fields = realloc(p_fields, size);
			if (p_fields == NULL) {
				throw_out_of_memory(L, "HTTP fields");
			}
			cdata->fields_data.pointer = p_fields;
		}
		char *p = p_fields + len;
		if (len) *p++ = '&';
		p = my_stpcpy(p, k_str);
		*p++ = '=';
		strcpy(p, v_estr);
		len += item_size;

		curl_free(v_estr);
		lua_pop(L, 1);
	}
	return len;
}

/**
 * Generates a mime data structure from a Lua key-value table and stores it into the socket structure
 *
 * @param lua_State*        L          Lua stack
 * @param int               data_index Stack position the mime data is
 * @param struct curl_data* cdata      Socket data structure
 *
 * @return long Length of the query data string
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static void set_http_mime(lua_State *L, int data_index, struct curl_data *cdata)
{
	cdata->multipart_data = curl_mime_init(cdata->handle);
	if (cdata->multipart_data == NULL) {
		throw_out_of_memory(L, "MIME init");
	}

	char numb_str[64];
	lua_pushnil(L);
	while (lua_next(L, data_index) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING) continue;
		const char *k_str = lua_tostring(L, -2);
		int v_type = lua_type(L, -1);
		const char *v_str;
		size_t v_size;
		switch (v_type) {
			case LUA_TSTRING:
				v_str = lua_tostring(L, -1);
				v_size = luaL_len(L, -1);
				break;
			case LUA_TBOOLEAN:
				if (lua_toboolean(L, -1)) {
					v_str = "true";
					v_size = 4;
				} else {
					v_str = "false";
					v_size = 5;
				}
				break;
			case LUA_TNUMBER:
				lua_pushvalue(L, -1);
				strcpy(numb_str, lua_tostring(L, -1));
				lua_pop(L, 1);
				v_str = numb_str;
				v_size = CURL_ZERO_TERMINATED;
				break;
			default:
				lua_pushfstring(L, "Incorrect type of the body value: %s [%s]", k_str, lua_typename(L, v_type));
				lua_error(L);
		}
		curl_mimepart *part = curl_mime_addpart(cdata->multipart_data);
		if (part == NULL) {
			throw_out_of_memory(L, "MIME addpart");
		}
		if (curl_mime_name(part, k_str) != CURLE_OK || curl_mime_data(part, v_str, v_size) != CURLE_OK) {
			throw_out_of_memory(L, "MIME data");
		}
		lua_pop(L, 1);
	}
}

/**
 * Sets the HTTP method and HTTP body from the given Lua params table
 *
 * @param lua_State*        L            Lua stack
 * @param int               params_index Stack position the params table is
 * @param struct curl_data* cdata        Socket data structure
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static void set_http_method_params(lua_State *L, int params_index, struct curl_data *cdata)
{
	int top = lua_gettop(L);

	int type = lua_getfield(L, params_index, "method");
	CURLcode res;
	const char* method;
	if (type != LUA_TSTRING || strcasecmp(method = lua_tostring(L, -1), "get") == 0) {
		res = curl_easy_setopt(cdata->handle, CURLOPT_HTTPGET, 1);
	} else if (strcasecmp(method, "post") == 0) {
		int bt_type = lua_getfield(L, params_index, "bodyType");
		const char *bt_val;
		if (bt_type == LUA_TNIL) {
			bt_val = NULL;
		} else if (bt_type == LUA_TSTRING) {
			bt_val = lua_tostring(L, -1);
		} else {
			lua_pushliteral(L, "Incorrect parameter bodyType");
			lua_error(L);
		}
		int bv_type = lua_getfield(L, params_index, "body");
		long size;
		if (bv_type == LUA_TNIL) {
			// Empty POST query
			size = 0;
			res = curl_easy_setopt(cdata->handle, CURLOPT_POSTFIELDS, "");
		} else if (bt_type == LUA_TNIL || strcasecmp(bt_val, "fields") == 0) {
			if (bv_type == LUA_TSTRING) {
				size = luaL_len(L, -1);
				cdata->fields_data.pointer = malloc(size);
				if (cdata->fields_data.pointer == NULL) {
					throw_out_of_memory(L, "HTTP");
				}
				memcpy(cdata->fields_data.pointer, lua_tostring(L, -1), size);
				cdata->fields_data.size = size;
				res = curl_easy_setopt(cdata->handle, CURLOPT_POSTFIELDS, cdata->fields_data.pointer);
			} else if (bv_type == LUA_TTABLE) {
				size = set_http_fields(L, lua_gettop(L), cdata);
				if (cdata->fields_data.pointer == NULL) {
					res = curl_easy_setopt(cdata->handle, CURLOPT_POSTFIELDS, "");
				} else {
					res = curl_easy_setopt(cdata->handle, CURLOPT_POSTFIELDS, cdata->fields_data.pointer);
				}
			} else {
				lua_pushliteral(L, "HTTP: Incorrect parameter body");
				lua_error(L);
			}
		} else if (strcasecmp(bt_val, "mime") == 0) {
			if (bv_type == LUA_TTABLE) {
				size = -1;
				set_http_mime(L, lua_gettop(L), cdata);
				res = curl_easy_setopt(cdata->handle, CURLOPT_MIMEPOST, cdata->multipart_data);
			} else {
				lua_pushliteral(L, "HTTP: Incorrect parameter body");
				lua_error(L);
			}
		} else {
			lua_pushfstring(L, "Unknown HTTP body type: %s", bt_val);
			lua_error(L);
		}

		if (res == CURLE_OK) {
			res = curl_easy_setopt(cdata->handle, CURLOPT_POSTFIELDSIZE, size);
		}
	} else {
		lua_pushfstring(L, "Unknown HTTP method: %s", method);
		lua_error(L);
	}

	if (res != CURLE_OK) {
		throw_setopt_error(L, res, get_error_string(res, cdata));
	}

	lua_settop(L, top);
}

/**
 * Performs general HTTP connection setting
 *
 * @param lua_State*        L     Lua stack
 * @param struct curl_data* cdata Socket data structure
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
static void set_general_options(lua_State *L, struct curl_data *cdata)
{
	CURLcode res;
	if ((res = curl_easy_setopt(cdata->handle, CURLOPT_HTTPHEADER, cdata->user_headers)) != CURLE_OK ||
		(res = curl_easy_setopt(cdata->handle, CURLOPT_WRITEFUNCTION, writefunction)) != CURLE_OK ||
		(res = curl_easy_setopt(cdata->handle, CURLOPT_WRITEDATA, cdata)) != CURLE_OK
	) {
		goto error;
	}

	if (!cdata->custom_useragent) {
		char app_ua[64];
		snprintf(app_ua, sizeof(app_ua), "%s/%s", APP_NAME, APP_VERSION);
		if ((res = curl_easy_setopt(cdata->handle, CURLOPT_USERAGENT, app_ua)) != CURLE_OK) {
			goto error;
		}
	}

	return;

error:
	throw_setopt_error(L, res, get_error_string(res, cdata));
}

/**
 * Updates the cookie engine contents and loads new contents from the current cookie jar when necessary
 *
 * @param lua_State* L            Lua stack
 * @param int        socket_index Stack position the socket object is at
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
static void select_cookiejar(lua_State *L, int socket_index)
{
	struct curl_data *cdata = lua_touserdata(L, socket_index);
	assert(cdata->handle != NULL);

	int start_pos = lua_gettop(L);
	lua_getuservalue(L, socket_index);

	bool enabled = (dataset_get_cookiejar(L, start_pos + 1, true) != LUA_TNIL);
	bool changed = dataset_apply_new_cookiejar_key(L, start_pos + 1);
	if (changed) {
		if (enabled) {
			libcurl_save_cookies(L, lua_gettop(L), cdata->handle);
			libcurl_clear_cookies(L, cdata->handle);
		}
		lua_pop(L, 1);

		if (dataset_is_cookiejar_active(L)) {
			libcurl_enable_cookies(L, true, cdata->handle);
			if (dataset_get_cookiejar(L, lua_gettop(L), false)) {
				libcurl_load_cookies(L, lua_gettop(L), cdata->handle);
			}
		} else {
			libcurl_enable_cookies(L, false, cdata->handle);
		}
	}

	lua_settop(L, start_pos);
}

/**
 * Returns the dataset table of the socket
 *
 * @param lua_State* L Lua stack
 *
 * @return void
 *
 * === Lua stack ===
 *
 * Input:
 *   - userdata Socket object
 *
 * Output:
 *   - table Table with lua values of the socket
 *
 */
void socket_get_dataset(lua_State *L)
{
	lua_getuservalue(L, -1);
}

/**
 * Forces socket cookies to be stored in the cookie jar.
 *
 * @param lua_State* L             Lua stack
 * @param int        socket_pos    Soket object position
 * @param int        cookiejar_pos Cookiejar position
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
void socket_flush_cookies(lua_State *L, int socket_pos, int cookiejar_pos)
{
	assert(lua_type(L, socket_pos) == LUA_TUSERDATA);
	assert(lua_type(L, cookiejar_pos) == LUA_TTABLE);

	struct curl_data *cdata = lua_touserdata(L, socket_pos);
	if (cdata->handle != NULL) {
		libcurl_save_cookies(L, cookiejar_pos, cdata->handle);
	}
}

/**
 * Updates cookies in the cookie engine of the socket
 *
 * @param lua_State* L             Lua stack
 * @param int        socket_pos    Soket object position
 * @param int        cookiejar_pos Cookiejar position
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
void socket_update_cookies(lua_State *L, int socket_pos, int cookiejar_pos)
{
	assert(lua_type(L, socket_pos) == LUA_TUSERDATA);
	assert(lua_type(L, cookiejar_pos) == LUA_TTABLE);

	struct curl_data *cdata = lua_touserdata(L, socket_pos);
	if (cdata->handle != NULL) {
		libcurl_clear_cookies(L, cdata->handle);
		libcurl_load_cookies(L, cookiejar_pos, cdata->handle);
	}
}

/**
 * Checks if the response subtable contains a key with the given name and its value is true
 *
 * @param lua_State* L            Lua stack
 * @param int        param_index The stack position the params table is at
 *
 * @return bool
 *
 * === Lua ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static bool http_want_list(lua_State *L, int params_index, const char *name)
{
	assert(lua_type(L, params_index) == LUA_TTABLE);

	bool res = false;
	if (lua_getfield(L, params_index, "response") == LUA_TTABLE) {
		lua_getfield(L, -1, name);
		if (lua_toboolean(L, -1)) res = true;
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return res;
}

/**
 * Checks if the cookie engine is active
 *
 * @param lua_State* L            Lua stack
 * @param int        socket_index Stack position the socket object is at
 *
 * @return bool
 *
 * === Lua ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static bool is_cookiejar_activated(lua_State *L, int socket_index)
{
	lua_getuservalue(L, socket_index);
	bool result = dataset_is_cookiejar_active(L);
	lua_pop(L, 1);
	return result;
}

/**
 * Runs before fetch handler of the module
 *
 * @param lua_State* L            Lua stack
 * @param int        socket_index Socket object position
 *
 * @return void
 *
 * === Lua ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static void before_fetch(lua_State *L, int socket_index)
{
	lua_getuservalue(L, socket_index);
	lua_getfield(L, -1, "beforefetch");
	lua_getfield(L, -2, "module");
	lua_pushvalue(L, socket_index);
	lua_call(L, 2, 0);
	lua_pop(L, 1);
}

/**
 * Performs an HTTP request to the specified resource
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata  Socket object
 *   - [2] string    URL
 *   - [3] table|nil Request parameters. The table may have the following keys:
 *                   - headers (table)     Key-value list of the request headers;
 *                   - method (string)     HTTP request method. The valid values are: "get" or "post".
 *                                         The default value is "get";
 *                   - body (table|string) Key-value array or string. The body with the MIME type
 *                                         must be presented as a table. Optional.
 *                   - bodyType (string)   "fields" or "mime". The default value is "fields".
 *                   - response (table)    Additional data that needs to be stored in the response.
 *                                         For instance: { headers = true }. The valid values are "headers", "cookies".
 *
 * Output:
 *   - userdata HTTP responce object
 */
static int fetch(lua_State *L)
{
	struct curl_data *cdata = check_curl_data_param(L, 1);
	const char *url = luaL_checkstring(L, 2);
	bool has_params = !lua_isnoneornil(L, 3);
	luaL_argcheck(L, !has_params || lua_istable(L, 3), 3, "table or nil expected");

	CURLcode res = curl_easy_setopt(cdata->handle, CURLOPT_URL, url);
	if (res != CURLE_OK) {
		throw_setopt_error(L, res, get_error_string(res, cdata));
	}

	if (!has_params) {
		lua_newtable(L);
		if (lua_gettop(L) > 3) lua_replace(L, 3);
	}

	free_request_outgoing_data(cdata);

	set_user_headers(L, 3, cdata);
	set_http_method_params(L, 3, cdata);
	set_general_options(L, cdata);

	if (cdata->new_cookiejar) {
		select_cookiejar(L, 1);
		cdata->new_cookiejar = false;
	}

	free_buffer(&cdata->writebuffer);

	cdata->error[0] = '\0';
	before_fetch(L, 1);
	res = curl_easy_perform(cdata->handle);
	++cdata->queries;

	free_request_outgoing_data(cdata);

	struct http_resp *resp = response_create(L);

	long http_code;
	if (res == CURLE_OK && (res = curl_easy_getinfo(cdata->handle, CURLINFO_RESPONSE_CODE, &http_code)) == CURLE_OK) {
		if (http_code >= 200 && http_code <= 299) resp->ok = true;
		resp->status = http_code;
		resp->buffer.pointer = cdata->writebuffer.pointer;
		resp->buffer.size = cdata->writebuffer.size;
		cdata->writebuffer.pointer = NULL;
		cdata->writebuffer.size = 0;
	} else {
		strncpy(resp->error, get_error_string(res, cdata), sizeof(resp->error));
		resp->error[sizeof(resp->error) - 1] = '\0';
	}
	if (http_want_list(L, 3, "headers")) {
		response_saveheaders(L, resp, cdata->handle);
	}
	if (is_cookiejar_activated(L, 1) && http_want_list(L, 3, "cookies")) {
		response_savecookies(L, resp, cdata->handle);
	}

	return 1;
}

/**
 * Converts the passed string to a URL encoded string and returns that
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] string String to escape
 *
 * Output:
 *   - string
 */
static int escape(lua_State *L)
{
	struct curl_data *cdata = check_curl_data_param(L, 1);
	const char *str = luaL_checkstring(L, 2);

	char *res_str = curl_easy_escape(cdata->handle, str, luaL_len(L, 2));
	if (res_str == NULL) {
		lua_pushliteral(L, "curl_easy_escape failed");
		lua_error(L);
	}
	lua_pushstring(L, res_str);
	curl_free(res_str);
	return 1;
}

/**
 * Converts the URL encoded string to a "plain string" and returns that
 *
 * @param lua_State* L Lua stack
 *
 * @return int 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] string String to unescape
 *
 * Output:
 *   - string
 */
static int unescape(lua_State *L)
{
	struct curl_data *cdata = check_curl_data_param(L, 1);
	const char *str = luaL_checkstring(L, 2);

	int res_len;
	char *res_str = curl_easy_unescape(cdata->handle, str, 0, &res_len);
	if (res_str == NULL) {
		lua_pushliteral(L, "curl_easy_unescape failed");
		lua_error(L);
	}
	lua_pushlstring(L, res_str, res_len);
	curl_free(res_str);
	return 1;
}

/**
 * Frees resources of the socket object
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Socket object
 *
 * Output:
 *   nothing
 */
static int cleanup(lua_State *L)
{
	struct curl_data *cdata = luaL_checkudata(L, 1, HTTP_SOCKET_META_NAME);
	if (cdata->handle == NULL) return 0;

	lua_getuservalue(L, 1); // [2] Socket data set
	if (dataset_is_cookiejar_global(L)) {
		if (dataset_get_cookiejar(L, 2, true) != LUA_TNIL) { // [3] Current cookie jar
			libcurl_save_cookies(L, 3, cdata->handle);
		}
		lua_pop(L, 1);
	}
	dataset_clear(L);
	curl_easy_cleanup(cdata->handle);
	cdata->handle = NULL;
	free_buffer(&cdata->writebuffer);
	free_request_outgoing_data(cdata);
	return 0;
}

/**
 * Implementation of the __index method for the socket userdata
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0 or 1
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Socket object
 *   - [2] string   Property name
 *
 * Output:
 *   - cfunction|number|nil
 */
static int get_property(lua_State *L)
{
	static const luaL_Reg flist[] = {
		{ "setOpt", set_opt },
		{ "setCookies", set_cookies },
		{ "fetch", fetch },
		{ "escape", escape },
		{ "unescape", unescape },
		{ "close", cleanup },
		{ NULL, NULL }
	};

	struct curl_data *cdata = lua_touserdata(L, 1);
	const char *pname = luaL_checkstring(L, 2);
	if (!get_cfunction(L, pname, flist)) {
		if (strcmp(pname, "queries") == 0) {
			lua_pushinteger(L, cdata->queries);
		} else {
			return 0;
		}
	}
	return 1;
}

/**
 * Implementation of the __newindex method for the socket userdata
 *
 * @param lua_State* L Lua stack
 *
 * @return int 0
 *
 * === Lua stack ===
 *
 * Input:
 *   - [1] userdata Socket object
 *   - [2] string   Property name
 *   - [3] any      Property value
 *
 * Output:
 *   nothing
 */
static int set_property(lua_State *L)
{
	struct curl_data *cdata = luaL_checkudata(L, 1, HTTP_SOCKET_META_NAME);
	const char *pname = luaL_checkstring(L, 2);
	if (strcmp(pname, "queries") == 0) {
		cdata->queries = luaL_checkinteger(L, 3);
	} else {
		lua_pushliteral(L, "Invalid socket property");
		lua_error(L);
	}
	return 0;
}

/**
 * Creates and initiates required structures
 *
 * @param lua_State *L Lua stack
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
void socket_init_structures(lua_State *L)
{
	if (luaL_newmetatable(L, HTTP_SOCKET_META_NAME)) {
		lua_pushcfunction(L, cleanup);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, get_property);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, set_property);
		lua_setfield(L, -2, "__newindex");
	}
	lua_pop(L, 1);

}

/**
 * Clears and frees lua data used for socket operations. It is called before module destruction
 *
 * @param lua_State *L Lua stack
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
void socket_cleanup_structures(lua_State *L)
{
	lua_pushliteral(L, HTTP_SOCKET_META_NAME);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
}
