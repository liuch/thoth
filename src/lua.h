#ifndef LUA_H
#define LUA_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <lua5.3/lauxlib.h>

void throw_out_of_memory(lua_State *L, const char *module);
int get_cfunction(lua_State *L, const char *name, const luaL_Reg *flist);

#ifdef __cplusplus
}
#endif
#endif // LUA_H

/* vim: set ft=c : */
