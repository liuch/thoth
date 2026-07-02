#include <stdbool.h>
#include <check.h>
#include "common.h"
#include "../src/json/json.h"

static void get_module(void)
{
	json_init(L);
}

static void call_method(const char *fname)
{
	get_module();
	lua_getfield(L, -1, fname);
	lua_insert(L, -3);
	lua_insert(L, -2);
	lua_call(L, 2, 1);
}

static void decode(void)
{
	call_method("decode");
}

static void encode(void)
{
	call_method("encode");
}

static void check_complex_table(void)
{
	int cnt = 0;
	int fkey = 0;
	int fval = 0;
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		const char *key = lua_tostring(L, -2);
		int tval = lua_type(L, -1);
		switch (*key) {
			case 'a':
				if (tval != LUA_TNUMBER || lua_tonumber(L, -1) != 1) fval++;
				break;
			case 'b':
				if (tval != LUA_TSTRING || strcmp("2", lua_tostring(L, -1)) != 0) fval++;
				break;
			case '3':
				if (tval != LUA_TBOOLEAN || lua_toboolean(L, -1) != false) fval++;
				break;
			case '7':
				if (tval != LUA_TBOOLEAN || lua_toboolean(L, -1) != true) fval++;
				break;
			default:
				fkey++;
				break;
		}
		cnt++;
		if (fkey) ck_abort_msg("Unknown key: %s", key);
		if (fval) ck_abort_msg("Incorrect value for key: %s", key);
		lua_pop(L, 1);
	}
	if (cnt != 4) ck_abort_msg("Incorrect the number of the keys");
}

START_TEST(test_json_get_module)
{
	get_module();
	check_userdata_meta_name("thoth.json");
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -1, "decode"));
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -2, "encode"));
}
END_TEST

START_TEST(test_json_decode_numbers_array_with_hole)
{
	lua_pushliteral(L, "[0, 1.01, null, 3, -4]");
	decode();
	ck_assert_int_eq(LUA_TTABLE, lua_type(L, -1));
	// Size of the table
	ck_assert_int_eq(5, luaL_len(L, -1));
	// Number of keys
	int cnt = 0;
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		++cnt;
		lua_pop(L, 1);
	}
	ck_assert_msg(cnt == 4, "Incorrect number of keys");
	// Values
	for (int i = 1; i <= 5; ++i) {
		lua_rawgeti(L, -1, i);
		if (i != 3) {
			ck_assert_msg(lua_type(L, -1) == LUA_TNUMBER, "Incorrect value type for key %d", i);
		}
		bool fail = false;
		switch (i) {
			case 1:
				if (lua_tonumber(L, -1) != 0) fail = true;
				break;
			case 2:
				if (lua_tonumber(L, -1) != 1.01) fail = true;
				break;
			case 3:
				if (!lua_isnil(L, -1)) fail = true;
				break;
			case 4:
				if (lua_tonumber(L, -1) != 3) fail = true;
				break;
			case 5:
				if (lua_tonumber(L, -1) != -4) fail = true;
				break;
		}
		lua_pop(L, 1);
		if (fail) ck_abort_msg("Incorrect value for key %d", i);
	}
}
END_TEST

START_TEST(test_json_decode_complex_array)
{
	lua_pushliteral(L, "{\"a\": 1, \"b\": \"2\", \"3\": false, \"7\": true}");
	decode();
	ck_assert_int_eq(LUA_TTABLE, lua_type(L, -1));
	check_complex_table();
}
END_TEST

START_TEST(test_json_encode_string)
{
	lua_pushliteral(L, "test string");
	encode();
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	ck_assert_str_eq("\"test string\"", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_json_encode_numbers_array)
{
	int i = 0;
	lua_newtable(L);
	lua_pushinteger(L, 0);   lua_rawseti(L, -2, ++i);
	lua_pushnumber(L, 1.01); lua_rawseti(L, -2, ++i);
	lua_pushinteger(L, 2);   lua_rawseti(L, -2, ++i);
	lua_pushinteger(L, 3);   lua_rawseti(L, -2, ++i);
	encode();
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	ck_assert_str_eq("[0, 1.01, 2, 3]", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_json_encode_numbers_array_with_hole)
{
	lua_newtable(L);
	lua_pushinteger(L, 0);   lua_rawseti(L, -2, 1);
	lua_pushnumber(L, 1.01); lua_rawseti(L, -2, 2);
	lua_pushnumber(L, 3.98); lua_rawseti(L, -2, 4);
	encode();
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	ck_assert_str_eq("[0, 1.01, null, 3.98]", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_json_encode_complex_array)
{
	lua_newtable(L);
	lua_pushinteger(L, 1);     lua_setfield(L, -2, "a");
	lua_pushliteral(L, "2");   lua_setfield(L, -2, "b");
	lua_pushboolean(L, false); lua_rawseti(L, -2, 3);
	lua_pushboolean(L, true);  lua_rawseti(L, -2, 7);
	encode();
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	// Decode the string back to check
	decode();
	check_complex_table();
}
END_TEST

START_TEST(test_json_encode_nested_arrays)
{
	int i = 0;
	lua_newtable(L);
	lua_pushinteger(L, 1);     lua_rawseti(L, -2, ++i);
	lua_pushboolean(L, true);  lua_rawseti(L, -2, ++i);
	lua_pushboolean(L, false); lua_rawseti(L, -2, ++i);
	lua_pushliteral(L, "2");   lua_rawseti(L, -2, ++i);
	int k = 0;
	lua_newtable(L);
	lua_pushinteger(L, 3);     lua_rawseti(L, -2, ++k);
	lua_pushboolean(L, false); lua_rawseti(L, -2, ++k);
	lua_pushboolean(L, true);  lua_rawseti(L, -2, ++k);
	lua_pushliteral(L, "0");   lua_rawseti(L, -2, ++k);
	lua_rawseti(L, -2, ++i);
	encode();
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	ck_assert_str_eq("[1, true, false, \"2\", [3, false, true, \"0\"]]", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_json_external_calls)
{
	const char *jstr = "{\"a\": 1, \"b\": \"2\", \"3\": false, \"7\": true}";
	json_decode_string(L, jstr, strlen(jstr));
	ck_assert_int_eq(LUA_TTABLE, lua_type(L, -1));
	check_complex_table();
}
END_TEST

Suite *json_suite(void)
{
	Suite *s = suite_create("Json");

	TCase *tc_module = tcase_create("Module");
	tcase_add_checked_fixture(tc_module, setup, teardown);
	tcase_add_test(tc_module, test_json_get_module);
	suite_add_tcase(s, tc_module);

	TCase *tc_decode = tcase_create("Decode");
	tcase_add_checked_fixture(tc_decode, setup, teardown);
	tcase_add_test(tc_decode, test_json_decode_numbers_array_with_hole);
	tcase_add_test(tc_decode, test_json_decode_complex_array);
	suite_add_tcase(s, tc_decode);

	TCase *tc_encode = tcase_create("Encode");
	tcase_add_checked_fixture(tc_encode, setup, teardown);
	tcase_add_test(tc_encode, test_json_encode_string);
	tcase_add_test(tc_encode, test_json_encode_numbers_array);
	tcase_add_test(tc_encode, test_json_encode_numbers_array_with_hole);
	tcase_add_test(tc_encode, test_json_encode_complex_array);
	tcase_add_test(tc_encode, test_json_encode_nested_arrays);
	suite_add_tcase(s, tc_encode);

	TCase *tc_external = tcase_create("External");
	tcase_add_checked_fixture(tc_external, setup, teardown);
	tcase_add_test(tc_external, test_json_external_calls);
	suite_add_tcase(s, tc_external);
	return s;
}
