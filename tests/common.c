#include <check.h>
#include "common.h"
#include "../src/lua.h"

lua_State *L;

void setup(void)
{
	L = luaL_newstate();
}

void teardown(void)
{
	lua_close(L);
}

void check_userdata_meta_name(const char *name)
{
	ck_assert_int_eq(LUA_TUSERDATA, lua_type(L, -1));
	ck_assert_int_eq(LUA_TSTRING, luaL_getmetafield(L, -1, "__name"));
	ck_assert_str_eq(name, lua_tostring(L, -1));
	lua_pop(L, 1);
}
