#include <stdbool.h>
#include <check.h>
#include "common.h"
#include "../src/lua.h"
#include "../src/core.h"

static void setup_core(void)
{
	setup();
	init_core(L);
}

static bool set_protection_state(bool new_state)
{
	lua_settop(L, 0);
	lua_pushboolean(L, new_state);
	ck_assert_int_eq(1, global_protect(L));
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	bool old_state = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return old_state;
}

START_TEST(test_core_stack_start_position)
{
	ck_assert_int_eq(0, lua_gettop(L));
}
END_TEST

START_TEST(test_core_get_global_protection_state)
{
	set_protection_state(true);
	ck_assert(set_protection_state(true) == true);
	ck_assert(set_protection_state(false) == true);
	ck_assert(set_protection_state(false) == false);
	ck_assert(set_protection_state(true) == false);
}
END_TEST

START_TEST(test_core_global_protection_turn_off)
{
	set_protection_state(false);
	luaL_loadstring(L, "var_name=111");
	ck_assert_int_eq(LUA_OK, lua_pcall(L, 0, 0, 0));

	lua_getglobal(L, "var_name");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(111, lua_tointeger(L, -1));
}
END_TEST

START_TEST(test_core_global_protection_turn_on)
{
	set_protection_state(true);
	luaL_loadstring(L, "var_name=222");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST

Suite *core_suite(void)
{
	Suite *s = suite_create("Json");

	TCase *tc_core = tcase_create("Core");
	tcase_add_checked_fixture(tc_core, setup_core, teardown);
	tcase_add_test(tc_core, test_core_stack_start_position);
	tcase_add_test(tc_core, test_core_get_global_protection_state);
	tcase_add_test(tc_core, test_core_global_protection_turn_off);
	tcase_add_test(tc_core, test_core_global_protection_turn_on);
	suite_add_tcase(s, tc_core);

	return s;
}
