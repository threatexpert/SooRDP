#pragma once
#include <winsock2.h>
#include <assert.h>
#include <map>
#include <atlbase.h>
#include <atlcore.h>
#include <atlthunk.h>
#include <ws2tcpip.h>

#ifdef _DEBUG
#define ASDbg  printf
#else
#define ASDbg(...)
#endif

#define ASWM_EVENT   WM_APP+1
#define ASWM_ACTIVE  WM_APP+2
#define ASWM_REMOVE  WM_APP+3
#define ASWM_DNS     WM_APP+4
#define ASWM_DNS2    WM_APP+5

#define AS_TIMER   1000

using namespace ATL;

class Casocket
{
    friend class CASocketMgr;
private:
    class CASocketMgr* m_pMgr;
    SOCKET m_key;
protected:
    HWND m_hwnd;
    struct astimer {
        bool valid;
        DWORD last;
        DWORD interval;
    };
    typedef std::map<int, astimer> ASTIMERS;
    ASTIMERS m_timers;
    bool m_timers_modified;
    char* m_pAsyncGetHostByNameBuffer;
    HANDLE m_hAsyncGetHostByNameHandle;
    int m_nAsyncGetHostByNamePort;
    HANDLE m_hAsyncGetAddrInfo;
    long m_lEvent;
public:
    enum {
        ERR_FAILED = -1,
        ERR_WOULDBLOCK = -8
    };
    SOCKET m_hSocket;
    BOOL m_isActived;
    BOOL m_bReleaseOnClose;
    Casocket();
    virtual ~Casocket();
    void Attach(SOCKET s);
    SOCKET Detach();
    BOOL CreateTCPSocket(bool overlapped, int af= AF_INET);
    BOOL Active();
    BOOL Select(long lEvent);
    BOOL Trigger(long lEvent, int err=0);

    virtual void Close();
    virtual void OnConnect(int err) {
    }
    virtual void OnAccept(int err) {
    }
    virtual void OnRead(int err) {
    }
    virtual void OnWrite(int err) {
    }
    virtual void OnClose(int err) {
        //可能会丢失这个事件，如果在OnRead或OnWrite中发现异常后直接调用Close，那么消息队列中如果有OnClose的消息则会因为句柄无效了而被忽略
    }
    virtual void OnRelease() {
        delete this;
    }
    virtual void OnTimer(int id) {
        //不要在这里面调用Close，应该Trigger个FD_CLOSE事件
    }
    virtual void Onresolved(const char* ip) {}

    bool Connect(const char* host, int port);
    bool ConnectEx(const char* host, int port);
    bool Connect(struct sockaddr* name, int namelen);
    int Write(const void* data, int len);

    bool setTimer(int id, DWORD uElapse);
    void killTimer(int id);

private:
    void dispTimer(DWORD tick);
};

typedef std::map<SOCKET, Casocket*> ASCONN;
class CASocketMgr
{
protected:
    AtlThunkData_t* m_thunk;
    ASCONN m_connmap;
    HWND m_hWnd;
    CComAutoCriticalSection m_lcmap;
    bool m_flag_cleaning;

    struct resolv
    {
        HANDLE thr;
        Casocket* s;
        HWND hwnd;
        UINT umsg;
        char name[256];
        int port;
        int family;
        int socktype;
        char ipstr[INET6_ADDRSTRLEN];
        sockaddr_storage addr;
        int addrlen;
        bool cancelled;
        bool result_ok;
    };
    typedef std::map<HANDLE, resolv*> ADNS;
    ADNS m_adns;
    CComAutoCriticalSection m_lcadns;

public:
    CASocketMgr()
    {
        m_thunk = AtlThunk_AllocateData();
        m_hWnd = NULL;
        m_flag_cleaning = false;
    }
    ~CASocketMgr()
    {
        Deinit();
        AtlThunk_FreeData(m_thunk);
    }

    BOOL Init() {

        WNDCLASSW wc = { };

        AtlThunk_InitData(m_thunk, sWindowProc, (size_t)this);

        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"Sample Window Class";
        RegisterClassW(&wc);
        m_hWnd = NULL;
        m_hWnd = CreateWindowW(wc.lpszClassName, L"",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, (HWND)NULL,
            (HMENU)NULL, NULL, (LPVOID)NULL);
        SetWindowLongPtr(m_hWnd,
            GWLP_WNDPROC, (LONG_PTR)AtlThunk_DataToCode(m_thunk));
        PostMessage(m_hWnd, WM_USER + 1, 0, 0);
        SetTimer(m_hWnd, AS_TIMER, 500, NULL);
        return TRUE;
    }

    void Deinit() {
        if (m_hWnd) {
            if (!DestroyWindow(m_hWnd)) {
                assert(false);
            }
            m_hWnd = NULL;
        }
        m_flag_cleaning = true;
        CComCritSecLock<CComAutoCriticalSection> lc(m_lcmap);
        for (ASCONN::iterator it = m_connmap.begin(); it != m_connmap.end(); it++) {
            it->second->OnRelease();
        }
        m_connmap.clear();
        CleanAsyncGetHostByName();
    }

    BOOL CreateSocket(Casocket* ins, long lEvent, int af=AF_INET)
    {
        if (ins->m_hSocket == INVALID_SOCKET) {
            if (!ins->CreateTCPSocket(true, af)) {
                return FALSE;
            }
        }
        return AddSocket(ins, lEvent);
    }

    BOOL AddSocket(Casocket* ins, long lEvent)
    {
        assert(ins->m_hSocket != INVALID_SOCKET);
        ASDbg("AddSocket:: ins=%p, fd=%d\n", ins, (int)ins->m_hSocket);
        if (ins->m_hSocket == INVALID_SOCKET)
            return FALSE;
        CComCritSecLock<CComAutoCriticalSection> lc(m_lcmap);
#ifdef _DEBUG
        ASCONN::iterator it = m_connmap.find(ins->m_hSocket);
        if (it != m_connmap.end()) {
            ASDbg("AddSocket:: ins=%p, fd=%d, EXIST!\n", ins, (int)ins->m_hSocket);
            //已经关闭且等待释放对象的ASWM_REMOVE消息估计在消息队列中了
        }
#endif
        m_connmap[ins->m_hSocket] = ins;
        ins->m_key = ins->m_hSocket;
        ins->m_pMgr = this;
        ins->m_hwnd = m_hWnd;
        if (!ins->Active() || !ins->Select(lEvent)) {
            ASDbg("AddSocket:: ins=%p, fd=%d, Active || Select failed!\n", ins, (int)ins->m_hSocket);
            m_connmap.erase(ins->m_hSocket);
            ins->m_key = INVALID_SOCKET;
            return FALSE;
        }
        return TRUE;
    }
    //不提前创建socket，这样Connect后DNS时可不仅限查ipv4的，如果结果是ipv6，再自动创建相应协议的socket
    BOOL AddingSocket(Casocket* ins, long lEvent)
    {
        ins->m_pMgr = this;
        ins->m_hwnd = m_hWnd;
        ins->m_lEvent = lEvent;
        return TRUE;
    }

    void RemoveSocket(Casocket* ins, BOOL bRelease) {
        {
            if (!m_flag_cleaning && ins->m_key != INVALID_SOCKET) {
                CComCritSecLock<CComAutoCriticalSection> lc(m_lcmap);
                m_connmap.erase(ins->m_key);
            }
        }
        ins->m_pMgr = NULL;
        if (bRelease) {
            ins->OnRelease();
        }
    }

    virtual void OnSockEvent(Casocket* s, long lEvent, int err)
    {
        switch (lEvent)
        {
        case FD_CONNECT:
        {
            s->OnConnect(err);
        }break;
        case FD_ACCEPT:
        {
            s->OnAccept(err);
        }break;
        case FD_READ:
        {
            s->OnRead(err);
        }break;
        case FD_WRITE:
        {
            s->OnWrite(err);
        }break;
        case FD_CLOSE:
        {
            s->OnClose(err);
        }break;
        }
    }

    virtual LRESULT CALLBACK OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == ASWM_EVENT) {
            ASCONN::iterator it = m_connmap.find((SOCKET)wParam);
            if (it == m_connmap.end()) {
                ASDbg("SocketEvent: not found, wparam=%d, lparam=%p\n", (int)wParam, (void*)lParam);
                return 0;
            }
            if (it->second->m_hSocket == INVALID_SOCKET) {
                ASDbg("SocketEvent: m_hSocket == INVALID_SOCKET, wparam=%d, lparam=%p\n", (int)wParam, (void*)lParam);
                return 0;
            }
            if (!it->second->m_isActived) {
                ASDbg("SocketEvent: instance not actived, wparam=%d, lparam=%p\n", (int)wParam, (void*)lParam);
                return 0;
            }
            OnSockEvent(it->second, WSAGETSELECTEVENT(lParam), WSAGETSELECTERROR(lParam));
            return 0;
        }
        else if (uMsg == ASWM_ACTIVE) {
            ASCONN::iterator it = m_connmap.find((SOCKET)wParam);
            if (it == m_connmap.end()) {
                return 0;
            }
            if (it->second == (Casocket*)lParam) {
                it->second->m_isActived = TRUE;
            }
            return 0;
        }
        else if (uMsg == ASWM_REMOVE) {
            Casocket* s = (Casocket*)lParam;
            if (wParam != INVALID_SOCKET){
                CComCritSecLock<CComAutoCriticalSection> lc(m_lcmap);
                ASCONN::iterator it = m_connmap.find((SOCKET)wParam);
                if (it == m_connmap.end()) {
                    return 0;
                }
                if (s == it->second) {
                    m_connmap.erase(it);
                }
            }
            s->m_pMgr = NULL;
            s->OnRelease();
            return 0;
        }
        else if (uMsg == WM_TIMER && wParam == AS_TIMER) {
            CComCritSecLock<CComAutoCriticalSection> lc(m_lcmap);
            DWORD dwTick = GetTickCount();
            for (ASCONN::iterator it = m_connmap.begin(); it != m_connmap.end(); it++) {
                it->second->dispTimer(dwTick);
            }
            return 0;
        }
        else if (uMsg == ASWM_DNS) {
            Casocket* pSocket = NULL;
            {
                CComCritSecLock<CComAutoCriticalSection> lc(m_lcmap);
                for (ASCONN::iterator it = m_connmap.begin(); it != m_connmap.end(); it++) {
                    pSocket = it->second;
                    if (pSocket && pSocket->m_hAsyncGetHostByNameHandle &&
                        pSocket->m_hAsyncGetHostByNameHandle == (HANDLE)wParam)
                        break;
                }            
            }
            if (!pSocket)
                return 0;

            int nErrorCode = WSAGETASYNCERROR(lParam);
            if (nErrorCode)
            {
                pSocket->OnConnect(nErrorCode);
                return 0;
            }
            in_addr addr;
            addr.s_addr = ((LPIN_ADDR)((LPHOSTENT)pSocket->m_pAsyncGetHostByNameBuffer)->h_addr)->s_addr;
            if (addr.s_addr == INADDR_NONE) {
                pSocket->OnConnect(-1);
                return 0;
            }
            pSocket->Onresolved(inet_ntoa(addr));
            bool res = pSocket->Connect(inet_ntoa(addr), pSocket->m_nAsyncGetHostByNamePort);
            delete[] pSocket->m_pAsyncGetHostByNameBuffer;
            pSocket->m_pAsyncGetHostByNameBuffer = 0;
            pSocket->m_hAsyncGetHostByNameHandle = 0;
            if (!res)
                pSocket->OnConnect(GetLastError());
            return 0;
        }
        else if (uMsg == ASWM_DNS2) {
            resolv* rs = (resolv*)lParam;
            {
                CComCritSecLock<CComAutoCriticalSection> lc(m_lcadns);
                m_adns.erase(rs->thr);
            }
            if (!rs->cancelled) {
                rs->s->m_hAsyncGetAddrInfo = NULL;
                if (rs->result_ok) {
                    rs->s->Onresolved(rs->ipstr);
                    bool res = rs->s->Connect((struct sockaddr*)&rs->addr, rs->addrlen);
                    if (!res)
                        rs->s->OnConnect(GetLastError());
                }
                else {
                    rs->s->OnConnect(-1);
                }
            }
            CloseHandle(rs->thr);
            delete rs;
        }
        return DefWindowProc(m_hWnd, uMsg, wParam, lParam);
    }

    static  LRESULT CALLBACK sWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        CASocketMgr* _this = (CASocketMgr*)hwnd;
        return _this->OnMessage(uMsg, wParam, lParam);
    }

    static DWORD WINAPI sthreat_resolver(void* param);
    HANDLE AsyncGetAddrInfo(Casocket* s, const char* name, int port);
    void CleanAsyncGetHostByName();
    void CancelAsyncGetHostByName(HANDLE h);
};

