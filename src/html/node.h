#ifndef HTMLNODE_H
#define HTMLNODE_H
#ifdef __cplusplus
extern "C"
{
#endif

#include <libxml2/libxml/HTMLparser.h>
#include "../lua.h"

void node_create(lua_State *L, int document_pos, xmlNodePtr node);
void node_init_structures(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // HTMLNODE_H
