#ifndef HTTP_LIBCURL_H
#define HTTP_LIBCURL_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <curl/curl.h>
#include "../lua.h"

void libcurl_enable_cookies(lua_State *L, bool enable, CURL *handle);
void libcurl_clear_cookies(lua_State *L, CURL *handle);
void libcurl_load_cookies(lua_State *L, int cookiejar_pos, CURL *handle);
void libcurl_save_cookies(lua_State *L, int cookiejar_pos, CURL *handle);

#ifdef __cplusplus
}
#endif
#endif // HTTP_LIBCURL_H
