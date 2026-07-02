#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <curl/curl.h>
#include "../lua.h"
#include "../common.h"

struct http_resp {
	bool   ok;
	int    status;
	char   error[CURL_ERROR_SIZE];
	struct buffer buffer;
	int    headers_ref;
	int    cookies_ref;
};

void response_init_structures(lua_State *L);
struct http_resp *response_create(lua_State *L);
void response_saveheaders(lua_State *L, struct http_resp *resp, CURL *handle);
void response_savecookies(lua_State *L, struct http_resp *resp, CURL *handle);

#ifdef __cplusplus
}
#endif
#endif // HTTP_RESPONSE_H
