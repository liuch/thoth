#ifndef SETTINGS_H
#define SETTINGS_H
#ifdef __cplusplus
extern "C"
{
#endif

struct settings {
	bool gprotect;
};

void init_settings(lua_State *L);
struct settings *get_settings(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // SETTINGS_H

