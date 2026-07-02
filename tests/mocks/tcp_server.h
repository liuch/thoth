#ifndef MOCK_TCP_SERVER_H
#define MOCK_TCP_SERVER_H
#ifdef __cplusplus
extern "C"
{
#endif

void tcpserver_start(const char *resp, size_t rsize);
int tcpserver_get_port(void);
const char *tcpserver_get_query(size_t *plen);
void tcpserver_stop(void);
void tcpserver_destroy(void);

#ifdef __cplusplus
}
#endif
#endif // MOCK_TCP_SERVER_H
