#ifndef HTMLERROR_H
#define HTMLERROR_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "../lua.h"

void throw_parser_error(lua_State *L, const char *err_string);

#ifdef __cplusplus
}
#endif
#endif // HTMLERROR_H
