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
	lua_pushfstring(L, "setopt: %s (%i)\n", error_message, error_code);
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
			lua_pushliteral(L, "Failed to set cookies");
			lua_error(L);
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
	if (curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) {
		lua_pushliteral(L, "curl_easy_getinfo failed");
		lua_error(L);
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
 * Stores cookies from the cookie engine to the jar table at the stack
 *
 * @param lua_State* L               Lua stack
 * @param int        cookiejar_index Cookie jar index
 * @param CURL*      handle          Socket handle
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
void cookiejar_from_curl(lua_State *L, int cookiejar_index, CURL *handle)
{
	assert(handle != NULL);

	struct curl_slist *cookies = NULL;
	if (curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) {
		lua_pushliteral(L, "curl_easy_getinfo failed");
		lua_error(L);
	}
	if (cookies != NULL) {
		int i = 0;
		struct curl_slist *it = cookies;
		do {
			lua_pushstring(L, it->data);
			lua_rawseti(L, cookiejar_index, ++i);
			it = it->next;
		} while (it != NULL);
		curl_slist_free_all(cookies);
	}
}

/**
 * Loads cookies from the jar table into the cookie engine
 *
 * @param lua_State* L               Lua stack
 * @param int        cookiejar_index Cookie jar index
 * @param CURL*      handle          Socket handle
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
void cookiejar_to_curl(lua_State *L, int cookiejar_index, CURL *handle)
{
	assert(handle != NULL);

	// Enumerate jar table
	for (int i = 1; ; ++i) {
		if (lua_rawgeti(L, cookiejar_index, i) == LUA_TNIL) break;
		// Insert cookie into the curl cookie engine
		CURLcode res = curl_easy_setopt(handle, CURLOPT_COOKIELIST, lua_tostring(L, -1));
		lua_pop(L, 1);
		if (res != CURLE_OK) {
			lua_pushliteral(L, "Failed to set cookies");
			lua_error(L);
		}
	}
}
