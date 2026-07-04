#include <check.h>
#include "common.h"
#include "../src/html/html.h"

static const char *html_str = "<html><body><span a=\"av\" b=\"bv\">content1</span><span c=\"cv\">content2</span><span>content3</span></body></html>";

static void get_module(void)
{
	html_init(L);
}

static void get_document(const char *str)
{
	get_module();
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -1, "fromString"));
	lua_rotate(L, -2, 1);
	lua_pushstring(L, str ? str : html_str);
	lua_call(L, 2, 1);
}

static void eval_xpath(const char *xpath_str)
{
	lua_getfield(L, -1, "evalXPath");
	lua_pushvalue(L, -2);
	lua_pushstring(L, xpath_str);
	lua_call(L, 2, 1);
	check_userdata_meta_name("thoth.html.xpath");
}

static int xres_size(void)
{
	lua_getfield(L, -1, "size");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	int res = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return res;
}

static const char *string_or_nil(void)
{
	int type = lua_type(L, -1);
	if (type == LUA_TNIL) {
		lua_pop(L, 1);
		return NULL;
	}
	if (type == LUA_TSTRING) {
		const char *res = lua_tostring(L, -1);
		luaL_ref(L, LUA_REGISTRYINDEX); // Avoid collecting by GC
		return res;
	}
	ck_abort();
}

static const char *xres_type(void)
{
	lua_getfield(L, -1, "type");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	return string_or_nil();
}

static void xres_result(int idx)
{
	int args = 1;
	lua_getfield(L, -1, "result");
	lua_pushvalue(L, -2);
	if (idx > 0) {
		lua_pushinteger(L, idx);
		args = 2;
	}
	lua_call(L, args, 1);
}

static const char *node_type(void)
{
	lua_getfield(L, -1, "type");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	return string_or_nil();
}

static const char *node_name(void)
{
	lua_getfield(L, -1, "name");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	return string_or_nil();
}

static const char *node_attribute(const char *name)
{
	lua_getfield(L, -1, "attribute");
	lua_pushvalue(L, -2);
	lua_pushstring(L, name);
	lua_call(L, 2, 1);
	return string_or_nil();
}

static const char *node_content()
{
	lua_getfield(L, -1, "textContent");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	return string_or_nil();
}

static const char *node_html()
{
	lua_getfield(L, -1, "outerHTML");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	return string_or_nil();
}

START_TEST(test_http_get_module)
{
	get_module();
	check_userdata_meta_name("thoth.html");
}
END_TEST

START_TEST(test_http_document)
{
	get_document(NULL);
	check_userdata_meta_name("thoth.html.doc");
}
END_TEST

START_TEST(test_html_document_element)
{
	get_document(NULL);
	lua_getfield(L, -1, "documentElement");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	check_userdata_meta_name("thoth.html.node");
}
END_TEST

START_TEST(test_html_xpath_get_node_list)
{
	get_document(NULL);
	eval_xpath("/html/body/span");
	ck_assert_int_eq(3, xres_size());
}
END_TEST

START_TEST(test_html_xpath_get_one_node)
{
	get_document(NULL);
	eval_xpath("/html/body/span[1]");
	ck_assert_int_eq(1, xres_size());
}
END_TEST

START_TEST(test_html_xpath_nested_query)
{
	get_document("<html><body><div><b></b></div><b></b><b></b></body></html>");
	eval_xpath("/html/body/div");
	xres_result(1);
	check_userdata_meta_name("thoth.html.node");
	eval_xpath(".//b");
	ck_assert_int_eq(1, xres_size());
}
END_TEST

START_TEST(test_html_xpath_get_element_by_attr)
{
	get_document("<a t=\"tv\"><b><c q=\"qv\"><d r=\"rv\"></d></c></b></a>");
	eval_xpath("//a[@t]//c[@q='qv']//d[contains(@r,'rv')]");
	ck_assert_int_eq(1, xres_size());
}
END_TEST

START_TEST(test_html_xpath_result_type)
{
	const char *list[] = {
		"//span",         "nodeset",
		"string(//span)", "string",
		"count(//span)",  "number",
		"true()",         "boolean",
		NULL
	};
	get_document(NULL);
	int top = lua_gettop(L);
	for (int i = 0; list[i]; i = i + 2) {
		eval_xpath(list[i]);
		ck_assert_str_eq(list[i + 1], xres_type());
		lua_settop(L, top);
	}
}
END_TEST

START_TEST(test_html_xpath_result_size)
{
	const char *query[] = { "//span", "//h1", "true()", NULL };
	const int  size[]   = { 3, 0, 0 };

	get_document(NULL);
	int top = lua_gettop(L);
	for (int i = 0; query[i]; ++i) {
		eval_xpath(query[i]);
		ck_assert_int_eq(size[i], xres_size());
		lua_settop(L, top);
	}
}
END_TEST

START_TEST(test_html_xpath_result_list)
{
	const int item_count = 3;
	get_document(NULL);
	eval_xpath("//span");
	xres_result(0);
	ck_assert_int_eq(LUA_TTABLE, lua_type(L, -1));
	ck_assert_int_eq(item_count, luaL_len(L, -1));
	for (int i = 1; i <= item_count; ++i) {
		lua_rawgeti(L, -1, i);
		check_userdata_meta_name("thoth.html.node");
		lua_pop(L, 1);
	}
}
END_TEST

START_TEST(test_html_xpath_result_string)
{
	get_document(NULL);
	eval_xpath("string(//span[3])");
	xres_result(0);
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	ck_assert_str_eq("content3", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_html_xpath_result_number)
{
	get_document(NULL);
	eval_xpath("count(//span)");
	xres_result(0);
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(3, lua_tointeger(L, -1));
}
END_TEST

START_TEST(test_html_xpath_result_boolean)
{
	get_document(NULL);
	eval_xpath("true()");
	xres_result(0);
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	ck_assert_int_eq(1, lua_toboolean(L, -1));
}
END_TEST

START_TEST(test_html_xpath_text_node_content)
{
	get_document(NULL);
	eval_xpath("//span[2]/text()");
	xres_result(1);
	check_userdata_meta_name("thoth.html.node");
	ck_assert_str_eq("content2", node_content());
}
END_TEST

START_TEST(test_html_xpath_attribute_content)
{
	get_document(NULL);
	eval_xpath("//span[2]/@c");
	xres_result(1);
	check_userdata_meta_name("thoth.html.node");
	ck_assert_str_eq("cv", node_content());
}
END_TEST

START_TEST(test_html_attr_get_from_real_node)
{
	get_document(NULL);
	eval_xpath("//span[1]");
	xres_result(1);
	ck_assert_str_eq("bv", node_attribute("b"));
}
END_TEST

START_TEST(test_html_attr_get_from_nonexistent_node)
{
	get_document(NULL);
	eval_xpath("//body");
	xres_result(1);
	ck_assert(node_attribute("a") == NULL);
}
END_TEST

START_TEST(test_html_attr_get_from_nonnode)
{
	get_document(NULL);
	eval_xpath("//span[@a]/@a");
	xres_result(1);
	ck_assert(node_attribute("a") == NULL);
}
END_TEST

START_TEST(test_html_node_get_type)
{
	const char *list[] = {
		"//span[1]",        "element",
		"//span[1]/text()", "text",
		"//span[1]/@a",     "attribute",
		NULL
	};
	get_document(NULL);
	int top = lua_gettop(L);
	for (int i = 0; list[i]; i = i + 2) {
		eval_xpath(list[i]);
		xres_result(1);
		check_userdata_meta_name("thoth.html.node");
		ck_assert_str_eq(list[i + 1], node_type());
		lua_settop(L, top);
	}
}
END_TEST

START_TEST(test_html_node_get_name)
{
	get_document(NULL);
	eval_xpath("//span[2]");
	xres_result(1);
	ck_assert_str_eq("span", node_name());
}
END_TEST

START_TEST(test_html_node_get_attribute_list)
{
	get_document(NULL);
	eval_xpath("//span[1]");
	xres_result(1);
	lua_getfield(L, -1, "attributes");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TTABLE, lua_type(L, -1));

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		ck_assert_int_eq(LUA_TSTRING, lua_type(L, -2));
		ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
		const char *name = lua_tostring(L, -2);
		if (strcmp(name, "a") == 0) {
			ck_assert_str_eq("av", lua_tostring(L, -1));
		} else if (strcmp(name, "b") == 0) {
			ck_assert_str_eq("bv", lua_tostring(L, -1));
		} else {
			ck_abort_msg("Unexpected attribute name: %s", name);
		}
		lua_pop(L, 1);
	}
}
END_TEST

START_TEST(test_html_node_get_text_content)
{
	get_document(NULL);
	eval_xpath("/html/body");
	xres_result(1);
	ck_assert_str_eq("content1content2content3", node_content());
}
END_TEST

START_TEST(test_html_node_get_html_content)
{
	get_document(NULL);
	eval_xpath("/html/body/span[1]");
	xres_result(1);
	ck_assert_str_eq("<span a=\"av\" b=\"bv\">content1</span>", node_html());
}
END_TEST

START_TEST(test_html_nodecmp_same_query)
{
	const char *query_str = "/html/body/span[1]";

	get_document(NULL);
	eval_xpath(query_str);
	xres_result(1);
	int node1 = lua_gettop(L);

	lua_pushvalue(L, 1);
	eval_xpath(query_str);
	xres_result(1);
	int node2 = lua_gettop(L);

	ck_assert(lua_compare(L, node1, node2, LUA_OPEQ));
}
END_TEST

START_TEST(test_html_nodecmp_different_queries)
{
	get_document(NULL);
	eval_xpath("/html/body/span[2]/@c");
	xres_result(1);
	int node1 = lua_gettop(L);

	lua_pushvalue(L, 1);
	eval_xpath("/html/body/span[@c]/@c");
	xres_result(1);
	int node2 = lua_gettop(L);

	ck_assert(lua_compare(L, node1, node2, LUA_OPEQ));
}
END_TEST

START_TEST(test_html_external_calls)
{
	html_get_document(L, html_str, strlen(html_str));
	check_userdata_meta_name("thoth.html.doc");
}
END_TEST

Suite *html_suite(void)
{
	Suite *s = suite_create("Html");

	TCase *tc_mod = tcase_create("Module");
	tcase_add_checked_fixture(tc_mod, setup, teardown);
	tcase_add_test(tc_mod, test_http_get_module);
	suite_add_tcase(s, tc_mod);

	TCase *tc_doc = tcase_create("Document");
	tcase_add_checked_fixture(tc_doc, setup, teardown);
	tcase_add_test(tc_doc, test_http_document);
	tcase_add_test(tc_doc, test_html_document_element);
	suite_add_tcase(s, tc_doc);

	TCase *tc_xpath = tcase_create("Xpath");
	tcase_add_checked_fixture(tc_xpath, setup, teardown);
	tcase_add_test(tc_xpath, test_html_xpath_get_node_list);
	tcase_add_test(tc_xpath, test_html_xpath_get_one_node);
	tcase_add_test(tc_xpath, test_html_xpath_nested_query);
	tcase_add_test(tc_xpath, test_html_xpath_get_element_by_attr);
	tcase_add_test(tc_xpath, test_html_xpath_result_type);
	tcase_add_test(tc_xpath, test_html_xpath_result_size);
	tcase_add_test(tc_xpath, test_html_xpath_result_list);
	tcase_add_test(tc_xpath, test_html_xpath_result_string);
	tcase_add_test(tc_xpath, test_html_xpath_result_number);
	tcase_add_test(tc_xpath, test_html_xpath_result_boolean);
	tcase_add_test(tc_xpath, test_html_xpath_text_node_content);
	tcase_add_test(tc_xpath, test_html_xpath_attribute_content);
	suite_add_tcase(s, tc_xpath);

	TCase *tc_attr = tcase_create("Attr");
	tcase_add_checked_fixture(tc_attr, setup, teardown);
	tcase_add_test(tc_attr, test_html_attr_get_from_real_node);
	tcase_add_test(tc_attr, test_html_attr_get_from_nonexistent_node);
	tcase_add_test(tc_attr, test_html_attr_get_from_nonnode);
	suite_add_tcase(s, tc_attr);

	TCase *tc_node = tcase_create("Node");
	tcase_add_checked_fixture(tc_node, setup, teardown);
	tcase_add_test(tc_node, test_html_node_get_type);
	tcase_add_test(tc_node, test_html_node_get_name);
	tcase_add_test(tc_node, test_html_node_get_attribute_list);
	tcase_add_test(tc_node, test_html_node_get_text_content);
	tcase_add_test(tc_node, test_html_node_get_html_content);
	suite_add_tcase(s, tc_node);

	TCase *tc_ncmp = tcase_create("Node cmp");
	tcase_add_checked_fixture(tc_ncmp, setup, teardown);
	tcase_add_test(tc_ncmp, test_html_nodecmp_same_query);
	tcase_add_test(tc_ncmp, test_html_nodecmp_different_queries);
	suite_add_tcase(s, tc_ncmp);

	TCase *tc_extern = tcase_create("External");
	tcase_add_checked_fixture(tc_extern, setup, teardown);
	tcase_add_test(tc_extern, test_html_external_calls);
	suite_add_tcase(s, tc_extern);

	return s;
}
