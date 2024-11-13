#include "pch.h"
#include "ASocket.h"

Casocket::Casocket()
{
    m_hwnd = NULL;
    m_key = m_hSocket = INVALID_SOCKET;
    m_isActived = FALSE;
    m_bReleaseOnClose = FALSE;
    m_timers_modified = false;
    m_pAsyncGetHostByNameBuffer = NULL;
    m_hAsyncGetHostByNameHandle = NULL;
    m_nAsyncGetHostByNamePort = 0;
    m_hAsyncGetAddrInfo = NULL;
    m_lEvent = 0;
}
Casocket::~Casocket() {
    Close();
    if (m_pMgr) {
        m_pMgr->RemoveSocket(this, FALSE);
    }
}
void Casocket::Attach(SOCKET s)
{
    m_hSocket = s;
}
SOCKET Casocket::Detach()
{
    SOCKET h = m_hSocket;
    m_hSocket = INVALID_SOCKET;
    return h;
}
BOOL Casocket::CreateTCPSocket(bool overlapped, int af)
{
    if (overlapped) {
        if ((m_hSocket = WSASocket(af, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
            return FALSE;
    }
    else {
        if ((m_hSocket = socket(af, SOCK_STREAM, 0)) == INVALID_SOCKET)
            return FALSE;
    }
    return TRUE;
}

BOOL Casocket::Active()
{
    if (!PostMessage(m_hwnd, ASWM_ACTIVE, m_hSocket, (LPARAM)this)) {
        return FALSE;
    }
    return TRUE;
}

BOOL Casocket::Select(long lEvent)
{

    if (0 != WSAAsyncSelect(m_hSocket, m_hwnd, ASWM_EVENT, lEvent)) {
        return FALSE;
    }
    return TRUE;
}

void Casocket::Close() {
    m_timers.clear();
    m_timers_modified = true;
    SOCKET s = m_hSocket;
    if (s != INVALID_SOCKET) {
        m_hSocket = INVALID_SOCKET;
        closesocket(s);
    }
    if (m_bReleaseOnClose) {
        m_bReleaseOnClose = FALSE;
        PostMessage(m_hwnd, ASWM_REMOVE, s, (LPARAM)this);
    }
    delete[] m_pAsyncGetHostByNameBuffer;
    m_pAsyncGetHostByNameBuffer = NULL;
    if (m_hAsyncGetHostByNameHandle)
        WSACancelAsyncRequest(m_hAsyncGetHostByNameHandle);
    m_hAsyncGetHostByNameHandle = NULL;
    if (m_hAsyncGetAddrInfo) {
        m_pMgr->CancelAsyncGetHostByName(m_hAsyncGetAddrInfo);
        m_hAsyncGetAddrInfo = NULL;
    }
}

BOOL Casocket::Trigger(long lEvent, int err) {
    BOOL ret = PostMessage(m_hwnd, ASWM_EVENT, m_hSocket, WSAMAKESELECTREPLY(lEvent, err));
    assert(ret);
    return ret;
}

static bool parse_addr(const char* host, int port, sockaddr_storage* ss, int* plen)
{
    typedef 
    INT WSAAPI DEF_inet_pton(
        INT   Family,
        PCSTR pszAddrString,
        PVOID pAddrBuf
    );
    static bool initapi = false;
    static DEF_inet_pton* _inet_pton = NULL;
    if (!initapi) {
        *(void**)&_inet_pton = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "inet_pton");
        initapi = true;
    }
    if (!_inet_pton) {
        struct sockaddr_in* srv_addr = (struct sockaddr_in* )ss;
        srv_addr->sin_family = AF_INET;
        srv_addr->sin_addr.s_addr = inet_addr(host);
        srv_addr->sin_port = htons(port);
        if (srv_addr->sin_addr.s_addr == INADDR_NONE)
            return false;
        *plen = sizeof(struct sockaddr_in);
        return true;
    }
    const struct sockaddr* sa = NULL;
    int sa_len = 0;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;

    memset(&sin, 0, sizeof(sin));
    if (1 == _inet_pton(AF_INET, host, &sin.sin_addr)) {
        /* Got an ipv4 address. */
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);

        sa = (const struct sockaddr*)&sin;
        sa_len = sizeof(sin);
    }
    else {
        memset(&sin6, 0, sizeof(sin6));
        if (1 == _inet_pton(AF_INET6, host, &sin6.sin6_addr)) {
            /* Got an ipv6 address. */
            sin6.sin6_family = AF_INET6;
            sin6.sin6_port = htons(port);

            sa = (const struct sockaddr*)&sin6;
            sa_len = sizeof(sin6);
        }
    }

    if (sa) {
        memcpy(ss, sa, sa_len);
        *plen = sa_len;
        return true;
    }
    else {
        return false;
    }
}

static
int get_socket_af(SOCKET sock)
{
    if (sock == INVALID_SOCKET) {
        return AF_UNSPEC;
    }
    WSAPROTOCOL_INFO info;
    int len = sizeof(info);
    if (getsockopt(sock, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&info, &len) == 0)
    {
        return info.iAddressFamily;
    }
    return AF_INET;
}

bool Casocket::Connect(const char* host, int port)
{
    sockaddr_storage srv_addr = { 0 };
    int addrlen;

    if (!parse_addr(host, port, &srv_addr, &addrlen)) {
        if (m_pAsyncGetHostByNameBuffer)
            delete[] m_pAsyncGetHostByNameBuffer;
        m_pAsyncGetHostByNameBuffer = new char[MAXGETHOSTSTRUCT];
        m_nAsyncGetHostByNamePort = port;

        m_hAsyncGetHostByNameHandle = WSAAsyncGetHostByName(m_hwnd, ASWM_DNS, host, m_pAsyncGetHostByNameBuffer, MAXGETHOSTSTRUCT);
        if (!m_hAsyncGetHostByNameHandle)
            return false;
        return true;
    }

    if (m_hSocket == INVALID_SOCKET) {
        if ((m_hSocket = WSASocket(srv_addr.ss_family, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
            return false;
        if (!m_pMgr->AddSocket(this, m_lEvent))
            return false;
    }
    if (SOCKET_ERROR != connect(m_hSocket, (struct sockaddr*)&srv_addr, addrlen))
        return false;
    if (WSAGetLastError() != WSAEWOULDBLOCK)
        return false;

    return true;
}

bool Casocket::ConnectEx(const char* host, int port)
{
    sockaddr_storage srv_addr = { 0 };
    int addrlen;

    if (!parse_addr(host, port, &srv_addr, &addrlen)) {
        m_hAsyncGetAddrInfo = m_pMgr->AsyncGetAddrInfo(this, host, port);
        return m_hAsyncGetAddrInfo != NULL;
    }

    if (m_hSocket == INVALID_SOCKET) {
        if ((m_hSocket = WSASocket(srv_addr.ss_family, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
            return false;
        if (!m_pMgr->AddSocket(this, m_lEvent))
            return false;
    }
    if (SOCKET_ERROR != connect(m_hSocket, (struct sockaddr*)&srv_addr, addrlen))
        return false;
    if (WSAGetLastError() != WSAEWOULDBLOCK)
        return false;

    return true;
}

bool Casocket::Connect(struct sockaddr* name, int namelen)
{
    if (m_hSocket == INVALID_SOCKET) {
        if ((m_hSocket = WSASocket(name->sa_family, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
            return false;
        if (!m_pMgr->AddSocket(this, m_lEvent))
            return false;
    }

    if (SOCKET_ERROR != connect(m_hSocket, name, namelen))
        return false;
    if (WSAGetLastError() != WSAEWOULDBLOCK)
        return false;

    return true;

}

int Casocket::Write(const void* data, int len) {
    int ret;
    ret = send(m_hSocket, (char*)data, len, 0);
    if (ret == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return ERR_WOULDBLOCK;
        }
        return ERR_FAILED;
    }
    return ret;
}

bool Casocket::setTimer(int id, DWORD uElapse) {
    for (ASTIMERS::iterator it = m_timers.begin(); it != m_timers.end(); it++) {
        if (it->first == id) {
            astimer& ast = it->second;
            ast.valid = true;
            ast.last = GetTickCount();
            ast.interval = uElapse;
            return true;
        }
    }
    astimer ast;
    ast.valid = true;
    ast.last = GetTickCount();
    ast.interval = uElapse;
    m_timers[id] = ast;
    m_timers_modified = true;
    return true;
}
void Casocket::killTimer(int id) {
    ASTIMERS::iterator it = m_timers.find(id);
    if (it != m_timers.end()) {
        it->second.valid = false;
    }
}

void Casocket::dispTimer(DWORD tick) {
    m_timers_modified = false;
    for (ASTIMERS::iterator it = m_timers.begin(); !m_timers_modified && it != m_timers.end(); ) {
        int id = it->first;
        astimer& ast = it->second;
        if (!ast.valid) {
            it = m_timers.erase(it);
        }
        else {
            ++it;
            if (ast.valid && tick - ast.last >= ast.interval) {
                ast.last = tick;
                OnTimer(id);
            }
        }
    }
}

//

void CASocketMgr::CancelAsyncGetHostByName(HANDLE h) {
    CComCritSecLock<CComAutoCriticalSection> lc(m_lcadns);
    ADNS::iterator it = m_adns.find(h);
    if (it == m_adns.end()) {
        return;
    }
    resolv* rs = it->second;
    rs->cancelled = true;
}

void CASocketMgr::CleanAsyncGetHostByName() {
    CComCritSecLock<CComAutoCriticalSection> lc(m_lcadns);
    for (ADNS::iterator it = m_adns.begin(); it != m_adns.end(); it++) {
        resolv* rs = it->second;
        if (WaitForSingleObject(it->first, 0) == WAIT_TIMEOUT) {
            TerminateThread(it->first, -1);
            WaitForSingleObject(it->first, INFINITE);
        }
        CloseHandle(it->first);
        delete rs;
    }
    m_adns.clear();
}

HANDLE CASocketMgr::AsyncGetAddrInfo(Casocket* s, const char* name, int port) {
    CComCritSecLock<CComAutoCriticalSection> lc(m_lcadns);
    resolv* rs = new resolv;
    memset(rs, 0, sizeof(*rs));
    rs->s = s;
    rs->hwnd = m_hWnd;
    rs->umsg = ASWM_DNS2;
    strcpy_s(rs->name, name);
    rs->port = port;
    rs->family = get_socket_af(s->m_hSocket);
    rs->socktype = SOCK_STREAM;
    int nTypeLen = sizeof(int);
    getsockopt(s->m_hSocket, SOL_SOCKET, SO_TYPE, (char*)&rs->socktype, &nTypeLen);

    DWORD dwTid;
    HANDLE h = CreateThread(0, 0, sthreat_resolver, rs, 0, &dwTid);
    if (h) {
        rs->thr = h;
        m_adns[h] = rs;
    }
    else {
        delete rs;
    }
    return h;
}

DWORD WINAPI CASocketMgr::sthreat_resolver(void* param)
{
    typedef
        PCSTR WSAAPI DEF_inet_ntop(
            INT        Family,
            const VOID* pAddr,
            PSTR       pStringBuf,
            size_t     StringBufSize
        );
    static bool initapi = false;
    static DEF_inet_ntop* _inet_ntop = NULL;
    if (!initapi) {
        *(void**)&_inet_ntop = GetProcAddress(GetModuleHandleA("ws2_32.dll"), "inet_ntop");
        initapi = true;
    }
    resolv* rs = (resolv*)param;
    char szport[64];
    struct addrinfo hints, * res, * p;
    int status;
    memset(&hints, 0, sizeof hints);
    int af = AF_INET;
    //OSVERSIONINFO osvi;
    //ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    //osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    //GetVersionEx(&osvi);
    //if (osvi.dwMajorVersion >= 6) {
    //    af = AF_UNSPEC;
    //}
    hints.ai_family = rs->family;
    hints.ai_socktype = rs->socktype;
    sprintf_s(szport, "%d", rs->port);
    BOOL isOK = FALSE;
    if ((status = getaddrinfo(rs->name, szport, &hints, &res)) == 0) {
        for (p = res; p != NULL; p = p->ai_next) {
            void* addr;
            if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                rs->addrlen = (int)p->ai_addrlen;
                memcpy_s(&rs->addr, sizeof(rs->addr), ipv4, p->ai_addrlen);
                addr = &(ipv4->sin_addr);
                if (_inet_ntop)
                    _inet_ntop(p->ai_family, addr, rs->ipstr, sizeof(rs->ipstr));
                else
                    strcpy_s(rs->ipstr, inet_ntoa(*(in_addr*)addr));
                rs->result_ok = true;
                break;
            }
            else if (p->ai_family == AF_INET6) { // IPv6
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
                rs->addrlen = (int)p->ai_addrlen;
                memcpy_s(&rs->addr, sizeof(rs->addr), ipv6, p->ai_addrlen);
                addr = &(ipv6->sin6_addr);
                if (_inet_ntop) {
                    _inet_ntop(p->ai_family, addr, rs->ipstr, sizeof(rs->ipstr));
                    rs->result_ok = true;
                    break;
                }
                else {
                    rs->ipstr[0] = '\0';
                }
            }
        }
        freeaddrinfo(res);
    }
    PostMessage(rs->hwnd, rs->umsg, 0, (LPARAM)rs);
    return 0;
}