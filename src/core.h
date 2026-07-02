#ifndef CORE_H
#define CORE_H
#ifdef __cplusplus
extern "C"
{
#endif

void init_core(lua_State *L);
int global_protect(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // CORE_H
