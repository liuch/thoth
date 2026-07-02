#ifndef CHECK_COMMON_H
#define CHECK_COMMON_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "../src/lua.h"

extern lua_State *L;

void setup(void);
void teardown(void);
void check_userdata_meta_name(const char *name);

#ifdef __cplusplus
}
#endif
#endif // CHECK_COMMON_H

