# The Thoth Project

LUA_LIB_FLAGS = -llua5.3
CURL_LIB_FLAGS = -lcurl
JANSSON_LIB_FLAGS = -ljansson
BASE64_LIB_FLAGS = -lb64
XML2_LIB_FLAGS = -I/usr/include/libxml2/ -lxml2
READLINE_LIB_FLAGS = -lreadline
LIBRARY_FLAGS = $(LUA_LIB_FLAGS) $(CURL_LIB_FLAGS) $(JANSSON_LIB_FLAGS) $(BASE64_LIB_FLAGS) $(XML2_LIB_FLAGS) $(READLINE_LIB_FLAGS)

TARGET_FILE = thoth
SRC_FILES = src/main.c src/common.c src/core.c src/settings.c
SRC_FILES += src/http/module.c src/http/socket.c src/http/dataset.c src/http/response.c src/http/libcurl.c
SRC_FILES += src/html/html.c src/html/node.c src/html/xpath.c src/html/error.c
SRC_FILES += src/json/json.c src/readline/readline.c src/utils/utils.c src/lua.c src/modules.c

TEST_SRC = tests/start.c tests/common.c tests/check_core.c tests/check_utils.c tests/check_json.c tests/check_html.c tests/check_http.c tests/check_modules.c
TEST_SRC += tests/mocks/tcp_server.c
TEST_SRC += src/common.c src/core.c src/settings.c
TEST_SRC += src/http/module.c src/http/socket.c src/http/dataset.c src/http/response.c src/http/libcurl.c
TEST_SRC += src/html/html.c src/html/node.c src/html/xpath.c src/html/error.c
TEST_SRC += src/json/json.c src/readline/readline.c src/utils/utils.c src/lua.c src/modules.c
TEST_BINARY = start_tests

CC ?= gcc
CFLAGS ?= -Wall -Wextra
PREFIX ?= /usr/local

.PHONY: all release debug executable ccheck install uninstall test

all: release

release: CFLAGS += -DNDEBUG
release: executable

debug: CFLAGS += -gdwarf
debug: executable

executable: $(SRC_FILES)
	$(CC) $(CFLAGS) $(SRC_FILES) -o $(TARGET_FILE) $(LIBRARY_FLAGS)

ccheck:
	cppcheck --inline-suppr --library=lua --check-level=exhaustive ./

install: $(TARGET_FILE)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -s $(TARGET_FILE) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(TARGET_FILE)

test: $(SRC_TESTS)
	$(CC) $(TEST_SRC) -o $(TEST_BINARY) $(LIBRARY_FLAGS) -lcheck -lsubunit -lm -D_REENTERANT -lpthread
	./$(TEST_BINARY)
