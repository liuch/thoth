#ifndef MODULES_H
#define MODULES_H
#ifdef __cplusplus
extern "C"
{
#endif

void get_modules(lua_State *L);
void unload_modules(void);

#ifdef __cplusplus
}
#endif
#endif // MODULES_H
