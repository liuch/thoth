#include <check.h>
#include "common.h"
#include "../src/lua.h"
#include "../src/modules.h"

START_TEST(test_module_list)
{
	const char *list[] = { "http", "html", "json", "utils", "readline" };
	lua_newtable(L);
	get_modules(L);
	for (int i = 0; i < sizeof(list) / sizeof(list[0]); ++i) {
		int res = lua_getfield(L, -1, list[i]);
		ck_assert_int_eq(LUA_TUSERDATA, res);
		lua_pop(L, 1);
	}
}
END_TEST

Suite *modules_suite(void)
{
	Suite *s = suite_create("Modules");

	TCase *tc_xpath = tcase_create("Main");
	tcase_add_checked_fixture(tc_xpath, setup, teardown);
	tcase_add_test(tc_xpath, test_module_list);
	suite_add_tcase(s, tc_xpath);

	return s;
}
