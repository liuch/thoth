#ifndef HTTP_SOCKET_H
#define HTTP_SOCKET_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "../lua.h"

void socket_create(lua_State *L);
void socket_get_dataset(lua_State *L);
void socket_flush_cookies(lua_State *L, int socket_pos, int cookiejar_pos);
void socket_update_cookies(lua_State *L, int socket_pos, int cookiejar_pos);
void socket_init_structures(lua_State *L);
void socket_cleanup_structures(lua_State *L);

#ifdef __cplusplus
}
#endif
#endif // HTTP_SOCKET_H
