#pragma once
#include <stdint.h>

BOOL socket_init_ws32();
SOCKET socket_createListener(DWORD dwIP, WORD wPort, int backlog, bool reuseaddr, bool overlapped);
SOCKET socket_connect(const char* ip, WORD port, int timeout_ms, bool overlapped);
BOOL socket_setbufsize(SOCKET s, int bufsize);
BOOL socket_mkpipes(SOCKET* pA, SOCKET* pB, bool overlapped);
BOOL socket_sendall(SOCKET s, const void* data, int len, int timeout_sec);
int socket_accept(SOCKET s, int nTimeOut, SOCKET *news, bool overlapped);
int socket_nonblocking(SOCKET sd, int enable);
int socket_close(SOCKET s);
int socket_err();
void socket_set_err(int e);
bool socket_would_block();

int util_inet_pton(int af, const char *src, void *dst);
const char * util_inet_ntop(int af, const void *src, char *dst, size_t size);
uint64_t _htonll(uint64_t host64);
uint64_t _ntohll(uint64_t net64);
