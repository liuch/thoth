#ifndef HTMLXPATH_H
#define HTMLXPATH_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <libxml2/libxml/xpath.h>
#include "../lua.h"

void xpath_create(lua_State *L, int document_pos);
void xpath_set_result(lua_State *L, int xpath_pos, xmlXPathObjectPtr result);
void xpath_init_structures(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // HTMLXPATH_H
