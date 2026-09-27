#ifndef JSON_H
#define JSON_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "../lua.h"

int json_init(lua_State *L);
int json_decode_string(lua_State *L, const char *str, size_t len);

#ifdef __cplusplus
}
#endif
#endif // JSON_H
