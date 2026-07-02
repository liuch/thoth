#include <check.h>
#include "common.h"
#include "../src/utils/utils.h"

static void get_module(void)
{
	utils_init(L);
}

static void get_method(const char *fname)
{
	get_module();
	lua_getfield(L, -1, fname);
	lua_replace(L, -2);
}

static void trim(const char *str)
{
	get_method("trim");
	lua_pushstring(L, str);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
}

static void base64dec(const char *str)
{
	get_method("base64dec");
	lua_pushstring(L, str);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
}

static void base64enc(const char *str, size_t len)
{
	get_method("base64enc");
	if (!len) len = strlen(str);
	lua_pushlstring(L, str, len);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
}

START_TEST(test_utils_get_module)
{
	get_module();
	check_userdata_meta_name("thoth.utils");
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -1, "trim"));
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -2, "base64dec"));
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -3, "base64enc"));
}
END_TEST

START_TEST(test_utils_trim_empty_string)
{
	trim("");
	ck_assert_str_eq("", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_trim_only_whitespaces)
{
	trim(" \n\r\t\v ");
	ck_assert_str_eq("", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_trim_whitespaces_left)
{
	trim(" \n\r\t\v test string");
	ck_assert_str_eq("test string", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_trim_whitespaces_right)
{
	trim("test string \n\r\t\v ");
	ck_assert_str_eq("test string", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_trim_whitespaces_left_right)
{
	trim(" \n\r\t\v test string \n\r\t\v ");
	ck_assert_str_eq("test string", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_trim_without_parameter)
{
	get_method("trim");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST

START_TEST(test_utils_trim_with_not_string)
{
	lua_pushnumber(L, 999);
	get_method("trim");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST

START_TEST(test_utils_b64dec_empty_string)
{
	base64dec("");
	ck_assert_str_eq("", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64dec_three_bytes)
{
	base64dec("TWFu");
	ck_assert_str_eq("Man", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64dec_two_bytes)
{
	base64dec("TWE=");
	ck_assert_str_eq("Ma", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64dec_one_byte)
{
	base64dec("TQ==");
	ck_assert_str_eq("M", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64dec_two_bytes_trim)
{
	base64dec("TWE");
	ck_assert_str_eq("Ma", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64dec_two_zero_bytes)
{
	base64dec("AAA=");
	ck_assert_int_eq(2, luaL_len(L, -1));
	const char *res = lua_tostring(L, -1);
	ck_assert(res[0] == '\0' && res[1] == '\0');
}
END_TEST

START_TEST(test_utils_b64dec_without_parameter)
{
	get_method("base64dec");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST

START_TEST(test_utils_b64dec_with_not_string)
{
	lua_pushnumber(L, 999);
	get_method("base64dec");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST

START_TEST(test_utils_b64enc_empty_string)
{
	base64enc("", 0);
	ck_assert_str_eq("", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64enc_three_bytes)
{
	base64enc("Man", 0);
	ck_assert_str_eq("TWFu", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64enc_two_bytes)
{
	base64enc("Ma", 0);
	ck_assert_str_eq("TWE=", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64enc_one_byte)
{
	base64enc("M", 0);
	ck_assert_str_eq("TQ==", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64enc_two_zero_bytes)
{
	base64enc("\0\0", 2);
	ck_assert_str_eq("AAA=", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_utils_b64enc_without_parameter)
{
	get_method("base64enc");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST

START_TEST(test_utils_b64enc_with_not_string)
{
	lua_pushnumber(L, 999);
	get_method("base64enc");
	ck_assert_int_ne(LUA_OK, lua_pcall(L, 0, 0, 0));
}
END_TEST


Suite *utils_suite(void)
{
	Suite *s = suite_create("Utils");

	TCase *tc_module = tcase_create("Trim");
	tcase_add_checked_fixture(tc_module, setup, teardown);
	tcase_add_test(tc_module, test_utils_get_module);
	suite_add_tcase(s, tc_module);

	TCase *tc_trim = tcase_create("Trim");
	tcase_add_checked_fixture(tc_trim, setup, teardown);
	tcase_add_test(tc_trim, test_utils_trim_empty_string);
	tcase_add_test(tc_trim, test_utils_trim_whitespaces_left);
	tcase_add_test(tc_trim, test_utils_trim_whitespaces_right);
	tcase_add_test(tc_trim, test_utils_trim_only_whitespaces);
	tcase_add_test(tc_trim, test_utils_trim_whitespaces_left_right);
	tcase_add_test(tc_trim, test_utils_trim_without_parameter);
	tcase_add_test(tc_trim, test_utils_trim_with_not_string);
	suite_add_tcase(s, tc_trim);

	TCase *tc_b64dec = tcase_create("B64dec");
	tcase_add_checked_fixture(tc_b64dec, setup, teardown);
	tcase_add_test(tc_b64dec, test_utils_b64dec_empty_string);
	tcase_add_test(tc_b64dec, test_utils_b64dec_three_bytes);
	tcase_add_test(tc_b64dec, test_utils_b64dec_two_bytes);
	tcase_add_test(tc_b64dec, test_utils_b64dec_one_byte);
	tcase_add_test(tc_b64dec, test_utils_b64dec_two_bytes_trim);
	tcase_add_test(tc_b64dec, test_utils_b64dec_two_zero_bytes);
	tcase_add_test(tc_b64dec, test_utils_b64dec_without_parameter);
	tcase_add_test(tc_b64dec, test_utils_b64dec_with_not_string);

	TCase *tc_b64enc = tcase_create("B64enc");
	tcase_add_checked_fixture(tc_b64enc, setup, teardown);
	tcase_add_test(tc_b64enc, test_utils_b64enc_empty_string);
	tcase_add_test(tc_b64enc, test_utils_b64enc_three_bytes);
	tcase_add_test(tc_b64enc, test_utils_b64enc_two_bytes);
	tcase_add_test(tc_b64enc, test_utils_b64enc_one_byte);
	tcase_add_test(tc_b64enc, test_utils_b64enc_two_zero_bytes);
	tcase_add_test(tc_b64enc, test_utils_b64enc_without_parameter);
	tcase_add_test(tc_b64enc, test_utils_b64enc_with_not_string);
	suite_add_tcase(s, tc_b64enc);

	return s;
}
