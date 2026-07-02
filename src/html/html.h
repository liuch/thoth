#ifndef HTML_H
#define HTML_H
#ifdef __cplusplus
extern "C"
{
#endif

void html_init(lua_State *L);
void html_getdocument(lua_State *L, const char *str, size_t len);

#ifdef __cplusplus
}
#endif
#endif // HTML_H
