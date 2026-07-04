#ifndef HTML_H
#define HTML_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <libxml2/libxml/HTMLparser.h>
#include "../lua.h"

void html_init(lua_State *L);
void html_get_document(lua_State *L, const char *str, size_t len);
void html_get_node_by_ptr(lua_State *L, int document_pos, xmlNodePtr node);
htmlDocPtr html_get_document_ptr(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // HTML_H
