#ifndef CORE_H
#define CORE_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "lua.h"

void init_core(lua_State *L);
void add_app_name(lua_State *L);
void add_app_version(lua_State *L);
int global_protect(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // CORE_H
