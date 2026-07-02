#ifndef HTTP_DATASET_H
#define HTTP_DATASET_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include "../lua.h"

void dataset_init(lua_State *L);
void dataset_clear(lua_State *L);
void dataset_get_module(lua_State *L);
bool dataset_is_cookiejar_active(lua_State *L);
bool dataset_is_cookiejar_global(lua_State *L);
void dataset_set_new_cookiejar_params(lua_State *L);
void dataset_get_active_cookiejar_params(lua_State *L);
int dataset_get_cookiejar(lua_State *L, int dataset_pos, bool empty);
bool dataset_apply_new_cookiejar_key(lua_State *L, int dataset_pos);

#ifdef __cplusplus
}
#endif
#endif // HTTP_DATASET_H
