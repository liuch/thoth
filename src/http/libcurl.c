#include <assert.h>
#include "libcurl.h"

/*
 * Throws an error related to setting connection options
 *
 * @param lua_State* L          Lua stack
 * @param int        error_code Error code
 *
 * @return void
 *
 * === Lua stacke ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   nothing
 */
static void throw_setopt_error(lua_State *L, int error_code)
{
	const char *error_message = curl_easy_strerror(error_code);
	lua_pushfstring(L, "Libcurl: setopt: %s (%i)", error_message, error_code);
	lua_error(L);
}

/*
 * Throws an error related to the curl_easy_getinfo() function
 *
 * @param lua_State* L          Lua stack
 * @param int        error_code Error code
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
static void throw_getinfo_error(lua_State *L, int error_code)
{
	const char *error_message = curl_easy_strerror(error_code);
	lua_pushfstring(L, "Libcurl: getinfo: %s (%i)", error_message, error_code);
	lua_error(L);
}

/*
 * Enables or disables the curl cookie engine
 *
 * @param lua_State* L      Lua stack
 * @param bool       enable Enable flag
 * @param CURL*      handle Curl handle to enable
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
void libcurl_enable_cookies(lua_State *L, bool enable, CURL *handle)
{
	assert(handle != NULL);

	const char *file_name = enable ? "" : NULL;
	CURLcode result;
	if ((result = curl_easy_setopt(handle, CURLOPT_COOKIEFILE, file_name)) != CURLE_OK) {
		throw_setopt_error(L, result);
	}
}

/**
 * Removes all cookies from the curl engine
 *
 * @param lua_State*        L     Lua stack
 * @param struct curl_data* cdata Raw structure of the socket object
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
void libcurl_clear_cookies(lua_State *L, CURL *handle)
{
	assert(handle != NULL);

	CURLcode result = curl_easy_setopt(handle, CURLOPT_COOKIELIST, "ALL");
	if (result != CURLE_OK) {
		throw_setopt_error(L, result);
	}
}

/**
 * Loads cookies from Lua table into the cookie engine
 *
 * @param lua_State* L             Lua stack
 * @param int        cookiejar_pos Cookie jar stack position
 * @param CURL*      handle        Curl handle
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
void libcurl_load_cookies(lua_State *L, int cookiejar_pos, CURL *handle)
{
	assert(handle != NULL);

	// Enumerate jar table
	for (int i = 1; ; ++i) {
		if (lua_rawgeti(L, cookiejar_pos, i) == LUA_TNIL) break;
		// Insert cookie int the curl cookie engine
		CURLcode result = curl_easy_setopt(handle, CURLOPT_COOKIELIST, lua_tostring(L, -1));
		lua_pop(L, 1);
		if (result != CURLE_OK) {
			throw_setopt_error(L, result);
		}
	}
}

/**
 * Stores cookies from the cookie engine to the jar table
 *
 * @param lua_State* L             Lua stack
 * @param int        cookiejar_pos Cookie jar stack position
 * @param CURL*      handle        Curl handle
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
void libcurl_save_cookies(lua_State *L, int cookiejar_pos, CURL *handle)
{
	assert(handle != NULL);

	struct curl_slist *cookies = NULL;
	int result = curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies);
	if (result != CURLE_OK) {
		throw_getinfo_error(L, result);
	}

	if (cookies != NULL) {
		int i = 0;
		struct curl_slist *it = cookies;
		do {
			lua_pushstring(L, it->data);
			lua_rawseti(L, cookiejar_pos, ++i);
			it = it->next;
		} while (it != NULL);
		curl_slist_free_all(cookies);
	}
}

/**
 * Converts the passed string to a URL encoded string
 *
 * @param lua_State*   L       Lua stack
 * @param const char*  str     Null-terminated string to escape
 * @param size_t       str_len String length
 * @param CURL*        handle  Curl handle
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   noting
 *
 * Output:
 *   - string
 */
int libcurl_escape(lua_State *L, const char *str, size_t str_len, CURL *handle)
{
	if (str_len > (size_t)INT_MAX) {
		lua_pushliteral(L, "Libcurl: the string is too long to escape");
		lua_error(L);
	}

	char *res_str = curl_easy_escape(handle, str, (int)str_len);
	if (res_str == NULL) {
		lua_pushliteral(L, "Libcurl: curl_easy_escape failed");
		lua_error(L);
	}

	lua_pushstring(L, res_str);
	curl_free(res_str);

	return 1;
}

/**
 * Converts the passed URL encoded string to a plain string
 *
 * @param lua_State*   L       Lua stack
 * @param const char*  str     Null-terminated string to unescape
 * @param size_t       str_len String length
 * @param CURL*        handle  Curl handle
 *
 * @return int
 *
 * === Lua stack ===
 *
 * Input:
 *   nothing
 *
 * Output:
 *   - string
 */
int libcurl_unescape(lua_State *L, const char *str, size_t str_len, CURL *handle)
{
	if (str_len > (size_t)INT_MAX) {
		lua_pushliteral(L, "Libcurl: the string is too long to unescape");
		lua_error(L);
	}

	int res_len;
	char *res_str = curl_easy_unescape(handle, str, (int)str_len, &res_len);
	if (res_str == NULL) {
		lua_pushliteral(L, "Libcurl: curl_easy_unescape failed");
		lua_error(L);
	}

	lua_pushlstring(L, res_str, (size_t)res_len);
	curl_free(res_str);

	return 1;
}
