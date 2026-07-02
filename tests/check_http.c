#include <check.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "../src/version.h"
#include "../src/http/module.h"
#include "mocks/tcp_server.h"

static void get_module(void)
{
	if (lua_getfield(L, LUA_REGISTRYINDEX, "test_module") == LUA_TNIL) {
		lua_pop(L, 1);
		http_init(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, "test_module");
	}
}

static void get_socket(void)
{
	get_module();
	ck_assert_int_eq(LUA_TFUNCTION, lua_getfield(L, -1, "getSocket"));
	lua_rotate(L, -2, 1);
	lua_call(L, 1, 1);
}

static void socket_close(void)
{
	lua_getfield(L, -1, "close");
	lua_pushvalue(L, -2);
	lua_call(L, 1, 0);
}

static int socket_str2str(const char *method)
{
	lua_getfield(L, -2, method);
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -3);
	lua_rotate(L, -3, 2);
	int res = lua_pcall(L, 2, 1, 0);
	if (res == LUA_OK) ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	return res;
}

static int socket_escape(void)
{
	return socket_str2str("escape");
}

static int socket_unescape(void)
{
	return socket_str2str("unescape");
}

static int socket_fetch(int num_params)
{
	int p_idx = lua_absindex(L, 0 - num_params);
	lua_getfield(L, p_idx - 1, "fetch");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, p_idx - 1);
	lua_rotate(L, p_idx, 2);
	int res = lua_pcall(L, num_params + 1, 1, 0);
	if (res == LUA_OK) ck_assert_int_eq(LUA_TUSERDATA, lua_type(L, -1));
	return res;
}

/**
 * @param int global. -1 - default, 0 - false, 1 - true
 */
static int socket_setcookiejar(const char *jarname, int global)
{
	lua_getfield(L, -1, "setCookies");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	if (jarname) {
		lua_pushstring(L, jarname);
	} else {
		lua_pushnil(L);
	}
	int num_params = 3;
	switch (global) {
		case -1:
			--num_params;
			break;
		case 0:
			lua_pushboolean(L, false);
			break;
		default:
			lua_pushboolean(L, true);
			break;
	}
	return lua_pcall(L, num_params, 2, 0);
}

static int socket_setopt(const char *name, const char *value)
{
	lua_getfield(L, -1, "setOpt");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	lua_pushstring(L, name);
	lua_pushstring(L, value);
	int res = lua_pcall(L, 3, 1, 0);
	if (res == LUA_OK) ck_assert_int_eq(LUA_TUSERDATA, lua_type(L, -1));
	return res;
}

static void push_url(void)
{
	lua_pushfstring(L, "http://127.0.0.1:%d/", tcpserver_get_port());
}

static bool resp_ok(void)
{
	lua_getfield(L, -1, "ok");
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	bool ok = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return ok;
}

static const char *resp_error(void)
{
	const char *error;
	lua_getfield(L, -1, "error");
	int type = lua_type(L, -1);
	if (type == LUA_TNIL) {
		error = NULL;
	} else {
		ck_assert_int_eq(LUA_TSTRING, type);
		error = lua_tostring(L, -1);
		luaL_ref(L, LUA_REGISTRYINDEX);
	}
	lua_pop(L, 1);
	return error;
}

static const char *resp_text(void)
{
	lua_getfield(L, -1, "text");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
	const char *text = lua_tostring(L, -1);
	luaL_ref(L, LUA_REGISTRYINDEX);
	return text;
}

static int resp_status(void)
{
	lua_getfield(L, -1, "status");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	int status = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return status;
}

/*
 * Makes { response = { <name> = true } } table and pushes it on the stack
 */
static void push_response_flag(const char *name)
{
	lua_newtable(L);
	lua_newtable(L); lua_pushboolean(L, true); lua_setfield(L, -2, name);
	lua_setfield(L, -2, "response");
}

static void resp_headers(const char *name)
{
	lua_getfield(L, -1, "headers");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	int pcnt = 1;
	if (name) {
		lua_pushstring(L, name);
		++pcnt;
	}
	lua_call(L, pcnt, 1);
	int type = lua_type(L, -1);
	if (name) {
		ck_assert(type == LUA_TSTRING || type == LUA_TNIL);
	} else {
		ck_assert(type == LUA_TTABLE || type == LUA_TNIL);
	}
}

static void resp_cookies(void)
{
	lua_getfield(L, -1, "cookies");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	int type = lua_type(L, -1);
	ck_assert(type == LUA_TTABLE || type == LUA_TNIL);
}

static bool cookie_exists(const char *name, const char *value)
{
	int size = luaL_len(L, -1);
	int nlen = strlen(name);
	for (int i = 1; i <= size; ++i) {
		lua_rawgeti(L, -1, i);
		ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
		const char *cstr = lua_tostring(L, -1);
		int tcnt = 5;
		for ( ; *cstr; ++cstr) if (*cstr == '\t' && --tcnt == 0) break;
		ck_assert_int_eq('\t', *cstr);
		int r = strncmp(++cstr, name, nlen);
		if (!r && cstr[nlen] == '\t') {
			cstr += nlen + 1;
			r = strcmp(cstr, value);
		}
		lua_pop(L, 1);
		if (!r) return true;
	}
	return false;
}

static bool cookie_exists_in_query(const char *name, const char *value, const char *query)
{
	char *pos = strcasestr(query, "\ncookie: ");
	if (!pos) return false;
	pos += 9;
	size_t nlen = strlen(name);
	if (strncmp(name, pos, nlen) != 0) return false;
	pos += nlen;
	if (*pos != '=') return false;
	size_t vlen = strlen(value);
	if (strncmp(value, ++pos, vlen) != 0) return false;
	pos += vlen;
	if (*pos != '\r' && *pos != '\n' && *pos != ';') return false;
	return true;
}

static bool header_exists_in_query(const char *name, const char *value, const char *query)
{
	const char *pos = query;
	int nlen = strlen(name);
	int vlen = strlen(value);
	int qlen = strlen(query);
	while ((pos = strcasestr(++pos, name)) != NULL) {
		if (pos > query && pos[-1] == '\n' && pos[nlen] == ':' && pos[nlen + 1] == ' ') {
			pos += nlen + 2;
			if (pos - query < qlen - vlen && strncmp(pos, value, vlen) == 0) {
				const char eol = pos[vlen];
				if (eol == '\n' || eol == '\r') return true;
			}
			break;
		}
	}
	return false;
}

START_TEST(test_http_get_module)
{
	get_module();
	check_userdata_meta_name("thoth.http");
}
END_TEST

START_TEST(test_http_open_close_socket)
{
	get_socket();
	check_userdata_meta_name("thoth.socket");
	socket_close();
}
END_TEST

START_TEST(test_http_double_close_socket)
{
	get_socket();
	socket_close();
	socket_close();
}

START_TEST(test_http_compare_sockets)
{
	get_socket();
	get_socket();
	ck_assert(!lua_compare(L, -1, -2, LUA_OPEQ));
}
END_TEST

START_TEST(test_http_initial_query_counter_value)
{
	get_socket();
	lua_getfield(L, -1, "queries");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(0, lua_tointeger(L, -1));
}
END_TEST

START_TEST(test_http_set_query_counter_value)
{
	get_socket();
	lua_pushinteger(L, 145);
	lua_setfield(L, -2, "queries");
	lua_getfield(L, -1, "queries");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(145, lua_tointeger(L, -1));
}
END_TEST

START_TEST(test_http_escape)
{
	get_socket();
	lua_pushlstring(L, "1q;?&= +\0", 9);
	ck_assert_int_eq(LUA_OK, socket_escape());
	ck_assert_str_eq("1q%3B%3F%26%3D%20%2B%00", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_http_unescape)
{
	get_socket();
	lua_pushliteral(L, "1q%3B%3F%26%3D%20%2B%00");
	ck_assert_int_eq(LUA_OK, socket_unescape());
	ck_assert_int_eq(9, luaL_len(L, -1));
	lua_pushlstring(L, "1q;?&= +\0", 9);
	ck_assert(lua_compare(L, -1, -2, LUA_OPEQ));
}
END_TEST

START_TEST(test_http_query_simple)
{
	tcpserver_start("HTTP/1.1 200 OK\nConnection: close\n\nHello!", 0);

	get_socket();
	lua_pushfstring(L, "http://127.0.0.1:%d/", tcpserver_get_port());
	socket_fetch(1);

	tcpserver_destroy();

	ck_assert(resp_ok());
	ck_assert(!resp_error());
	ck_assert_int_eq(200, resp_status());
	ck_assert_str_eq("Hello!", resp_text());
}
END_TEST

START_TEST(test_http_query_error)
{
	tcpserver_start("HTTP/1.1 404 Not Found\n\nPage not found", 0);

	get_socket();
	lua_pushfstring(L, "http://127.0.0.1:%d/", tcpserver_get_port());
	socket_fetch(1);

	tcpserver_destroy();

	ck_assert(!resp_ok());
	ck_assert_int_eq(404, resp_status());
}
END_TEST

START_TEST(test_http_query_counter_increment)
{
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);

	get_socket();
	lua_pushinteger(L, 111);
	lua_setfield(L, -2, "queries");
	push_url();
	socket_fetch(1);

	tcpserver_destroy();

	lua_getfield(L, -2, "queries");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(112, lua_tointeger(L, -1));
}
END_TEST

START_TEST(test_http_query_get_content_twice)
{
	tcpserver_start("HTTP/1.1 200 Ok\n\nHello!", 0);

	get_socket();
	push_url();
	socket_fetch(1);

	tcpserver_destroy();

	ck_assert_str_eq("Hello!", resp_text());
	ck_assert_str_eq("", resp_text());
}
END_TEST

START_TEST(test_http_query_get_html_content)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\n\n<!DOCTYPE html><html lang=\"en\"><body><div>Hello!</div></body></html>",
		0
	);

	get_socket();
	push_url();
	socket_fetch(1);

	tcpserver_destroy();

	lua_getfield(L, -1, "html");
	ck_assert_int_eq(LUA_TFUNCTION, lua_type(L, -1));
	lua_pushvalue(L, -2);
	lua_call(L, 1, 1);
	ck_assert_int_eq(LUA_TUSERDATA, lua_type(L, -1));
}
END_TEST

START_TEST(test_http_get_header_list)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nUser-Header1: Header1Value\nUser-Header2: Header2Value\n\nHello!",
		0
	);

	get_socket();
	push_url();
	push_response_flag("headers");
	socket_fetch(2);

	tcpserver_destroy();

	resp_headers(NULL);
	int keys_found = 0;
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		ck_assert_int_eq(LUA_TSTRING, lua_type(L, -2));
		ck_assert_int_eq(LUA_TSTRING, lua_type(L, -1));
		const char *hname = lua_tostring(L, -2);
		if (strcmp("user-header1", hname) == 0) {
			ck_assert_str_eq("Header1Value", lua_tostring(L, -1));
			++keys_found;
		} else if (strcmp("user-header2", hname) == 0) {
			ck_assert_str_eq("Header2Value", lua_tostring(L, -1));
			++keys_found;
		} else {
			ck_abort_msg("Unknown header: %s", hname);
		}
		lua_pop(L, 1);
	}
	ck_assert_int_eq(2, keys_found);
}
END_TEST

START_TEST(test_http_get_header_by_name)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nUser-Header1: Header1Value\nUser-Header2: Header2Value\n\nHello!",
		0
	);

	get_socket();
	push_url();
	push_response_flag("headers");
	socket_fetch(2);

	tcpserver_destroy();

	resp_headers("uSeR-hEaDeR1");
	ck_assert_str_eq("Header1Value", lua_tostring(L, -1));
	lua_pop(L, 1);
	resp_headers("uSeR-hEaDeR2");
	ck_assert_str_eq("Header2Value", lua_tostring(L, -1));
	lua_pop(L, 1);
	resp_headers("uSeR-hEaDeR9");
	ck_assert_int_eq(LUA_TNIL, lua_type(L, -1));
	lua_pop(L, 1);
}
END_TEST

START_TEST(test_http_get_header_list_without_param)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nUser-Header1: Header1Value\nUser-Header2: Header2Value\n\nHello!",
		0
	);

	get_socket();
	push_url();
	socket_fetch(1);

	tcpserver_destroy();

	resp_headers(NULL);
	ck_assert_int_eq(LUA_TNIL, lua_type(L, -1));
	lua_pop(L, 1);
}
END_TEST

START_TEST(test_http_get_header_by_name_without_param)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nUser-Header1: Header1Value\nUser-Header2: Header2Value\n\nHello!",
		0
	);

	get_socket();
	push_url();
	socket_fetch(1);

	tcpserver_destroy();

	resp_headers("User-Header1");
	ck_assert_int_eq(LUA_TNIL, lua_type(L, -1));
}
END_TEST

START_TEST(test_http_set_header)
{
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);

	get_socket();
	push_url();
	lua_newtable(L);
	lua_newtable(L);
	lua_pushliteral(L, "strval");
	lua_setfield(L, -2, "User-Header1");
	lua_pushinteger(L, -123);
	lua_setfield(L, -2, "User-Header2");
	lua_pushboolean(L, true);
	lua_setfield(L, -2, "User-Header3");
	lua_setfield(L, -2, "headers");
	socket_fetch(2);

	tcpserver_stop();

	const char *query = tcpserver_get_query(NULL);
	ck_assert(header_exists_in_query("User-Header1", "strval", query));
	ck_assert(header_exists_in_query("User-Header2", "-123", query));
	ck_assert(header_exists_in_query("User-Header3", "true", query));

	tcpserver_destroy();
}
END_TEST

START_TEST(test_http_set_useragent)
{
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);

	get_socket();
	ck_assert_int_eq(LUA_OK, socket_setopt("useragent", "my UA"));
	push_url();
	socket_fetch(1);

	tcpserver_stop();

	ck_assert(header_exists_in_query("user-agent", "my UA", tcpserver_get_query(NULL)));

	tcpserver_destroy();
}
END_TEST

START_TEST(test_http_set_useragent_with_headers)
{
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);

	get_socket();
	push_url();
	lua_newtable(L);
	lua_newtable(L);
	lua_pushliteral(L, "my UA");
	lua_setfield(L, -2, "User-Agent");
	lua_setfield(L, -2, "headers");
	socket_fetch(2);

	tcpserver_stop();

	const char *query = tcpserver_get_query(NULL);
	ck_assert(header_exists_in_query("user-agent", "my UA", query));

	tcpserver_destroy();
}
END_TEST

START_TEST(test_http_default_useragent)
{
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);

	get_socket();
	push_url();
	socket_fetch(1);

	tcpserver_stop();

	char app_str[32];
	snprintf(app_str, sizeof(app_str), "%s/%s", APP_NAME, APP_VERSION);
	ck_assert(header_exists_in_query("user-agent", app_str, tcpserver_get_query(NULL)));

	tcpserver_destroy();
}
END_TEST

START_TEST(test_http_get_cookies)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nSet-Cookie: cname1=cValue1\nSet-Cookie: cname2=cValue2\n\nHello!",
		0
	);

	get_socket();
	ck_assert_int_eq(LUA_OK, socket_setcookiejar("cjar", -1)); lua_pop(L, 2);
	push_url();
	push_response_flag("cookies");
	socket_fetch(2);

	tcpserver_destroy();

	resp_cookies();
	ck_assert_int_eq(LUA_TTABLE, lua_type(L, -1));
	ck_assert_int_eq(2, luaL_len(L, -1));
	ck_assert(cookie_exists("cname1", "cValue1"));
	ck_assert(cookie_exists("cname2", "cValue2"));
}
END_TEST

START_TEST(test_http_just_switch_cookie_jar)
{
	get_socket();

	ck_assert_int_eq(LUA_OK, socket_setcookiejar("local_jar1", -1));
	ck_assert_int_eq(LUA_TNIL, lua_type(L, -2));
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	ck_assert_int_eq(false, lua_toboolean(L, -1));
	lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();

	ck_assert_int_eq(LUA_OK, socket_setcookiejar("local_jar2_fake", 0));
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -2));
	ck_assert_str_eq("local_jar1", lua_tostring(L, -2));
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	ck_assert_int_eq(false, lua_toboolean(L, -1));
	lua_pop(L, 2);

	ck_assert_int_eq(LUA_OK, socket_setcookiejar("local_jar2", 0));
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -2));
	ck_assert_str_eq("local_jar1", lua_tostring(L, -2));
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	ck_assert_int_eq(false, lua_toboolean(L, -1));
	lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();

	ck_assert_int_eq(LUA_OK, socket_setcookiejar("global_jar3", 1));
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -2));
	ck_assert_str_eq("local_jar2", lua_tostring(L, -2));
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	ck_assert_int_eq(false, lua_toboolean(L, -1));
	lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();

	ck_assert_int_eq(LUA_OK, socket_setcookiejar(NULL, -1));
	ck_assert_int_eq(LUA_TSTRING, lua_type(L, -2));
	ck_assert_str_eq("global_jar3", lua_tostring(L, -2));
	ck_assert_int_eq(LUA_TBOOLEAN, lua_type(L, -1));
	ck_assert_int_eq(true, lua_toboolean(L, -1));
	lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();
}
END_TEST

START_TEST(test_http_send_query_with_different_jars)
{
	get_socket();

	// Set cookies for jar1
	socket_setcookiejar("jar1", -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\nSet-Cookie: cname1=c1\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();
	// Set cookies for jar2
	socket_setcookiejar("jar2", -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\nSet-Cookie: cname2=c2\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();
	// Check for cookies with no jar
	socket_setcookiejar(NULL, -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_stop();
	ck_assert(strcasestr(tcpserver_get_query(NULL), "\ncookie:") == 0);
	tcpserver_destroy();
	// Check for cookies with jar1
	socket_setcookiejar("jar1", -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_stop();
	ck_assert(cookie_exists_in_query("cname1", "c1", tcpserver_get_query(NULL)));
	ck_assert(!cookie_exists_in_query("cname2", "c2", tcpserver_get_query(NULL)));
	tcpserver_destroy();
	// Check for cookies with jar2
	socket_setcookiejar("jar2", -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_stop();
	ck_assert(!cookie_exists_in_query("cname1", "c1", tcpserver_get_query(NULL)));
	ck_assert(cookie_exists_in_query("cname2", "c2", tcpserver_get_query(NULL)));
	tcpserver_destroy();
}
END_TEST

static bool exchange_cookies_between_sockets(int public, bool close_first)
{
	// Set cookies for the first socket
	get_socket();
	socket_setcookiejar("jar_id", public); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\nSet-Cookie: cname1=c1\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();
	if (close_first) {
		socket_close();
	}
	// Check cookies for the second socket with the same jar id
	get_socket();
	socket_setcookiejar("jar_id", public); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_stop();

	return cookie_exists_in_query("cname1", "c1", tcpserver_get_query(NULL));
}

START_TEST(test_http_use_same_jar_id_with_different_sockets)
{
	ck_assert(!exchange_cookies_between_sockets(-1, false));
	ck_assert(!exchange_cookies_between_sockets(-1, true));
	ck_assert(!exchange_cookies_between_sockets(0, false));
	ck_assert(!exchange_cookies_between_sockets(0, true));
	ck_assert(exchange_cookies_between_sockets(1, false));
	ck_assert(exchange_cookies_between_sockets(1, true));
}
END_TEST

START_TEST(test_http_disable_cookie_engine)
{
	get_socket();

	// Set some cookies
	socket_setcookiejar("jar3", -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\nSet-Cookie: cname3=c3\nSet-Cookie: cname4=c4\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_destroy();
	// Disable the cookie engine
	socket_setcookiejar(NULL, -1); lua_pop(L, 2);
	tcpserver_start("HTTP/1.1 200 OK\n\nHello!", 0);
	push_url();
	socket_fetch(1); lua_pop(L, 1);
	tcpserver_stop();
	ck_assert(!cookie_exists_in_query("cname3", "c3", tcpserver_get_query(NULL)));
	ck_assert(!cookie_exists_in_query("cname4", "c4", tcpserver_get_query(NULL)));
	tcpserver_destroy();
}
END_TEST

START_TEST(test_http_get_cookies_without_jar_key)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nSet-Cookie: cname1=cValue1\nSet-Cookie: cname2=cValue2\n\nHello!",
		0
	);

	get_socket();
	push_url();
	push_response_flag("cookies");
	socket_fetch(2);

	tcpserver_destroy();

	resp_cookies();
	ck_assert_int_eq(LUA_TNIL, lua_type(L, -1));
}
END_TEST

START_TEST(test_http_get_cookies_without_params)
{
	tcpserver_start(
		"HTTP/1.1 200 OK\nSet-Cookie: cname1=cValue1\nSet-Cookie: cname2=cValue2\n\nHello!",
		0
	);

	get_socket();
	ck_assert_int_eq(LUA_OK, socket_setcookiejar("cjar", -1)); lua_pop(L, 2);
	push_url();
	socket_fetch(1);

	tcpserver_destroy();

	resp_cookies();
	ck_assert_int_eq(LUA_TNIL, lua_type(L, -1));
}
END_TEST


START_TEST(test_http_close_twice)
{
	get_socket(); socket_close();
	socket_close();
}
END_TEST

START_TEST(test_http_set_cookies_when_closed)
{
	get_socket(); socket_close();
	ck_assert_int_ne(LUA_OK, socket_setcookiejar("cjar", -1));
	ck_assert_str_eq("The socket is closed", lua_tostring(L, -1));
	lua_pop(L, 2);
}
END_TEST

START_TEST(test_http_query_when_closed)
{
	get_socket(); socket_close();
	push_url();
	ck_assert_int_ne(LUA_OK, socket_fetch(1));
	ck_assert_str_eq("The socket is closed", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_http_set_opt_when_closed)
{
	get_socket(); socket_close();
	ck_assert_int_ne(LUA_OK, socket_setopt("useragent", "my UA"));
	ck_assert_str_eq("The socket is closed", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_http_query_counter_when_closed)
{
	get_socket();

	socket_close();
	lua_getfield(L, -1, "queries");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(0, lua_tointeger(L, -1));
	lua_pop(L, 1);

	lua_pushinteger(L, 100);
	lua_setfield(L, -2, "queries");
	socket_close();
	lua_getfield(L, -1, "queries");
	ck_assert_int_eq(LUA_TNUMBER, lua_type(L, -1));
	ck_assert_int_eq(100, lua_tointeger(L, -1));
	lua_pop(L, 1);
}
END_TEST

START_TEST(test_http_escape_when_closed)
{
	get_socket(); socket_close();
	lua_pushstring(L, "");
	ck_assert_int_ne(LUA_OK, socket_escape());
	ck_assert_str_eq("The socket is closed", lua_tostring(L, -1));
}
END_TEST

START_TEST(test_http_unescape_when_closed)
{
	get_socket(); socket_close();
	lua_pushstring(L, "");
	ck_assert_int_ne(LUA_OK, socket_unescape());
	ck_assert_str_eq("The socket is closed", lua_tostring(L, -1));
}
END_TEST


Suite *http_suite(void)
{
	Suite *s = suite_create("Http");

	TCase *tc_main = tcase_create("Main");
	tcase_add_checked_fixture(tc_main, setup, teardown);
	tcase_add_test(tc_main, test_http_get_module);
	tcase_add_test(tc_main, test_http_open_close_socket);
	tcase_add_test(tc_main, test_http_double_close_socket);
	tcase_add_test(tc_main, test_http_compare_sockets);
	tcase_add_test(tc_main, test_http_initial_query_counter_value);
	tcase_add_test(tc_main, test_http_set_query_counter_value);
	suite_add_tcase(s, tc_main);

	TCase *tc_utils = tcase_create("Utils");
	tcase_add_checked_fixture(tc_utils, setup, teardown);
	tcase_add_test(tc_utils, test_http_escape);
	tcase_add_test(tc_utils, test_http_unescape);
	suite_add_tcase(s, tc_utils);

	TCase *tc_getq = tcase_create("Get queries");
	tcase_add_checked_fixture(tc_getq, setup, teardown);
	tcase_add_test(tc_getq, test_http_query_simple);
	tcase_add_test(tc_getq, test_http_query_error);
	tcase_add_test(tc_getq, test_http_query_counter_increment);
	tcase_add_test(tc_getq, test_http_query_get_content_twice);
	tcase_add_test(tc_getq, test_http_query_get_html_content);
	suite_add_tcase(s, tc_getq);

	TCase *tc_headers = tcase_create("Headers");
	tcase_add_checked_fixture(tc_headers, setup, teardown);
	tcase_add_test(tc_headers, test_http_get_header_list);
	tcase_add_test(tc_headers, test_http_get_header_by_name);
	tcase_add_test(tc_headers, test_http_get_header_list_without_param);
	tcase_add_test(tc_headers, test_http_get_header_by_name_without_param);
	tcase_add_test(tc_headers, test_http_set_header);
	suite_add_tcase(s, tc_headers);

	TCase *tc_options = tcase_create("Options");
	tcase_add_checked_fixture(tc_options, setup, teardown);
	tcase_add_test(tc_options, test_http_set_useragent);
	tcase_add_test(tc_options, test_http_set_useragent_with_headers);
	tcase_add_test(tc_options, test_http_default_useragent);
	suite_add_tcase(s, tc_options);

	TCase *tc_cookies = tcase_create("Cookies");
	tcase_add_checked_fixture(tc_cookies, setup, teardown);
	tcase_add_test(tc_cookies, test_http_get_cookies);
	tcase_add_test(tc_cookies, test_http_just_switch_cookie_jar);
	tcase_add_test(tc_cookies, test_http_send_query_with_different_jars);
	tcase_add_test(tc_cookies, test_http_use_same_jar_id_with_different_sockets);
	tcase_add_test(tc_cookies, test_http_disable_cookie_engine);
	tcase_add_test(tc_cookies, test_http_get_cookies_without_jar_key);
	tcase_add_test(tc_cookies, test_http_get_cookies_without_params);
	suite_add_tcase(s, tc_cookies);

	TCase *tc_closed = tcase_create("Closed");
	tcase_add_checked_fixture(tc_closed, setup, teardown);
	tcase_add_test(tc_closed, test_http_close_twice);
	tcase_add_test(tc_closed, test_http_set_cookies_when_closed);
	tcase_add_test(tc_closed, test_http_query_when_closed);
	tcase_add_test(tc_closed, test_http_set_opt_when_closed);
	tcase_add_test(tc_closed, test_http_query_counter_when_closed);
	tcase_add_test(tc_closed, test_http_escape_when_closed);
	tcase_add_test(tc_closed, test_http_unescape_when_closed);
	suite_add_tcase(s, tc_closed);

	return s;
}
