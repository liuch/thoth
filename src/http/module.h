#ifndef HTTP_MODULE_H
#define HTTP_MODULE_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "../lua.h"

int http_init(lua_State *L);
void module_get_cookiejar_storage(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // HTTP_MODULE_H
