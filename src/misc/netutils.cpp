#include "pch.h"
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2ipdef.h>
#include <Ws2tcpip.h>
#include "netutils.h"

#pragma comment(lib, "ws2_32")

enum {
	fds_READ = 1,
	fds_WRITE = 1<<1,
	fds_EXCEPT = 1<<2,
};

BOOL socket_init_ws32()
{
	WSADATA wsadata;
	return WSAStartup(MAKEWORD(2,2),&wsadata) == 0;
}


int socket_close(SOCKET s)
{
	return ::closesocket(s);
}

SOCKET create_tcp_socket(bool nonblocking, bool overlapped)
{
	SOCKET sd;

	if (overlapped) {
		if ((sd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
			return INVALID_SOCKET;
	}
	else {
		if ((sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
			return INVALID_SOCKET;
	}
#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
	// We do not want SIGPIPE if writing to socket.
	const int value = 1;
	if (0 != setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(int)))
	{
		closeSocket(sd);
		return INVALID_SOCKET;
	}
#endif

	if (nonblocking)
	{
		if (0 != socket_nonblocking(sd, true))
		{
			socket_close(sd);
			return INVALID_SOCKET;
		}
	}

	return sd;
}

int socket_err()
{
	return WSAGetLastError();
}

void socket_set_err(int e)
{
	WSASetLastError(e);
}

bool socket_would_block()
{
	return (WSAGetLastError() == WSAEWOULDBLOCK);
}


int socket_select(SOCKET s, int fds, int timeout_msec)
{
	int ret;
	fd_set FdRead, FdWrite, FdExcept;
	struct timeval TimeOut;

	bool read = (fds & fds_READ) != 0;
	bool write = (fds & fds_WRITE) != 0;
	bool except = (fds & fds_EXCEPT) != 0;

	TimeOut.tv_sec = timeout_msec / 1000;
	TimeOut.tv_usec = (timeout_msec % 1000) * 1000;

	if (read)
	{
		FD_ZERO(&FdRead);
		FD_SET(s, &FdRead);
	}
	if (write)
	{
		FD_ZERO(&FdWrite);
		FD_SET(s, &FdWrite);
	}
	if (except)
	{
		FD_ZERO(&FdExcept);
		FD_SET(s, &FdExcept);
	}

	ret = select((int)s + 1,
		read ? &FdRead : NULL,
		write ? &FdWrite : NULL,
		except ? &FdExcept : NULL,
		timeout_msec == -1 ? NULL : &TimeOut);

	if (ret > 0)
	{
		ret = 0;

		if (read && FD_ISSET(s, &FdRead))
			ret |= fds_READ;

		if (write && FD_ISSET(s, &FdWrite))
			ret |= fds_WRITE;

		if (except && FD_ISSET(s, &FdExcept))
		{
			int nErr;
			int optlen = sizeof(int);
			if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&nErr, &optlen) == 0)
			{
				SetLastError(nErr);
			}

			ret |= fds_EXCEPT;
		}
	}

	return ret;
}


bool socket_connect2(SOCKET s, const struct sockaddr* addr, int addrlen, int timeout_msec /*= -1*/)
{
	if (connect(s, (const struct sockaddr*)addr, addrlen) == SOCKET_ERROR)
	{
		if (!socket_would_block())
			goto error;
	}

	if (socket_select(s, fds_WRITE|fds_EXCEPT, timeout_msec) != fds_WRITE)
	{
		goto error;
	}

	return true;
error:
	return false;
}

SOCKET socket_connect(const char* ip, WORD port, int timeout_ms, bool overlapped)
{
	SOCKET sockid;
	int errn;
	if ((sockid = create_tcp_socket(true, overlapped)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	struct sockaddr_in srv_addr = { 0 };
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = inet_addr(ip);
	srv_addr.sin_port = htons(port);

	if (!socket_connect2(sockid, (struct sockaddr*)&srv_addr, sizeof(struct sockaddr_in), timeout_ms))
	{
		goto error;
	}

	return sockid;
error:
	errn = socket_err();
	socket_close(sockid);
	socket_set_err(errn);
	return INVALID_SOCKET;
}

BOOL socket_setbufsize(SOCKET s, int bufsize)
{
	int val = bufsize;
	int r1 = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&val, sizeof(int));
	int r2 = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&val, sizeof(int));

	return r1 == 0 && r2 == 0;
}

BOOL socket_mkpipes(SOCKET* pA, SOCKET* pB, bool overlapped)
{
	SOCKET svr = INVALID_SOCKET, sA = INVALID_SOCKET, sB = INVALID_SOCKET;
	WORD port;
	sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	svr = socket_createListener(inet_addr("127.0.0.1"), 0, 1, false, overlapped);
	if (svr == INVALID_SOCKET)
		goto _ERROR;
	if (getsockname(svr, (sockaddr*)&addr, &addrlen) == SOCKET_ERROR)
		goto _ERROR;
	port = ntohs(addr.sin_port);
	sA = socket_connect("127.0.0.1", port, 2000, overlapped);
	if (sA == INVALID_SOCKET)
		goto _ERROR;
	if (socket_accept(svr, 2, &sB, overlapped) <= 0)
		goto _ERROR;

	closesocket(svr);
	*pA = sA;
	*pB = sB;
	return TRUE;
_ERROR:
	if (svr != INVALID_SOCKET)
		closesocket(svr);
	if (sA != INVALID_SOCKET)
		closesocket(sA);
	if (sB != INVALID_SOCKET)
		closesocket(sB);
	return FALSE;
}

BOOL socket_sendall(SOCKET s, const void* data, int len, int timeout_sec)
{
	int ret;
	int bs;
	while (len > 0) {
		bs = min(len, 1024 * 64);
		ret = (int)send(s, (char*)data, bs, 0);
		if (ret < 0)
		{
			if (!socket_would_block())
				return FALSE;
			ret = socket_select(s + 1, fds_WRITE, timeout_sec);
			if (ret <= 0) {
				return -1;
			}
			continue;
		}
		else if (ret == 0) {
			return FALSE;
		}
		data = (const char*)data + ret;
		len -= ret;
	}
	return TRUE;
}

SOCKET socket_createListener(DWORD dwIP, WORD wPort, int backlog, bool reuseaddr, bool overlapped)
{
	SOCKET sockid;
	int errn;
	if ((sockid = create_tcp_socket(true, overlapped)) == INVALID_SOCKET)
		return INVALID_SOCKET;

	int flag = 1, len = sizeof(int);
	if (reuseaddr)
	{
		setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, len);
	}

	struct sockaddr_in srv_addr = { 0 };
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = dwIP;
	srv_addr.sin_port = htons(wPort);

	if (::bind(sockid, (struct sockaddr*)&srv_addr, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		goto error;
	}

	if (::listen(sockid, backlog) == SOCKET_ERROR)
	{
		goto error;
	}

	return sockid;
error:
	errn = socket_err();
	socket_close(sockid);
	socket_set_err(errn);
	return INVALID_SOCKET;
}

int socket_nonblocking(SOCKET sd, int enable)
{
	int iMode = 1;//nonzero
	return ioctlsocket(sd, FIONBIO, (u_long FAR*) &iMode);//Enabled Nonblocking Mode
}

int socket_accept(SOCKET s, int nTimeOut, SOCKET *news, bool overlapped)
{
	int ret;
	struct timeval TimeOut;
	fd_set FdRead;
	FD_ZERO(&FdRead);
	FD_SET(s, &FdRead);
	TimeOut.tv_sec = nTimeOut;
	TimeOut.tv_usec = 0;
	if ((ret = select((int)s + 1, &FdRead, NULL, NULL, nTimeOut == -1 ? NULL : &TimeOut)) <= 0) {
		return ret == 0 ? 0 : -1;
	}

	SOCKET sd;
	if (overlapped) {
		sd = WSAAccept(s, NULL, NULL, NULL, 0);
	}
	else {
		sd = accept(s, NULL, NULL);
	}
	if (sd != INVALID_SOCKET)
	{

		if (0 != socket_nonblocking(sd, 1))
		{
			socket_close(sd);
			return -1;
		}

#if defined(SO_NOSIGPIPE) && !defined(MSG_NOSIGNAL)
		// We do not want SIGPIPE if writing to socket.
		const int value = 1;
		if (0 != setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(int)))
		{
			socket_close(sd);
			return -1;
		}
#endif
	}
	*news = sd;
	return 1;
}


int util_inet_pton(int af, const char *src, void *dst)
{
	if (af == AF_INET)
	{
		if (inet_addr(src) != INADDR_NONE)
		{
			((IN_ADDR*)dst)->s_addr = inet_addr(src);
			return 1;
		}
		return 0;
	}
	else if (af == AF_INET6)
	{
		typedef INT WSAAPI D_inet_pton(INT Family, PCSTR pszAddrString, PVOID pAddrBuf);
		static D_inet_pton *p_inet_pton = NULL;
		static BOOL p_inet_pton_init = FALSE;
		if (!p_inet_pton_init)
		{
			HMODULE hws = GetModuleHandleA("ws2_32");
			if (hws)
				*(void**)&p_inet_pton = GetProcAddress(hws, "inet_pton");

			p_inet_pton_init = TRUE;
		}

		if (p_inet_pton)
		{
			return p_inet_pton(af, src, dst);
		}

		return -1;
	}
	else
		return -1;
}

const char * util_inet_ntop(int af, const void *src, char *dst, size_t size)
{
	if (af == AF_INET)
	{
		char *p = inet_ntoa(*(in_addr*)src);
		if (p)
		{
			strncpy_s(dst, size, p, size);
			p = dst;
		}
		return p;
	}
	else if (af == AF_INET6)
	{
		typedef PCSTR WSAAPI D_inet_ntop( INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
		static D_inet_ntop *p_inet_ntop = NULL;
		static BOOL p_inet_ntop_init = FALSE;
		if (!p_inet_ntop_init)
		{
			HMODULE hws = GetModuleHandleA("ws2_32");
			if (hws)
				*(void**)&p_inet_ntop = GetProcAddress(hws, "inet_ntop");

			p_inet_ntop_init = TRUE;
		}

		if (p_inet_ntop)
		{
			return p_inet_ntop(af, (PVOID)src, dst, size);
		}

		return NULL;
	}
	else
		return NULL;
}
