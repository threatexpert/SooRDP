#include "pch.h"
#include "TSChannel.h"
#include <atlbase.h>
#include <atlthunk.h>
#include <windows.h>
#include <wtsapi32.h>
#include <pchannel.h>
#include "../misc/Thread.h"
#include "../misc/netutils.h"
#include "../SooRDP-Plugin/idata.h"
#include <map>
#include "../misc/SoTunnel.h"

#ifdef _DEBUG
#define Dbg  printf
#else
#define Dbg(...)
#endif

typedef
HANDLE WINAPI DEF_xWTSVirtualChannelOpenEx(
    DWORD SessionId,
    LPSTR pVirtualName,
    DWORD flags
);
DEF_xWTSVirtualChannelOpenEx* gpWTSVirtualChannelOpenEx = NULL;

BOOL supportDynamicVirtualChannel()
{
    HMODULE hMod = GetModuleHandleA("Wtsapi32.dll");
    if (hMod == NULL) {
        hMod = LoadLibraryA("Wtsapi32.dll");
        if (hMod == NULL) {
            return FALSE;
        }
    }

    *(void**)&gpWTSVirtualChannelOpenEx = GetProcAddress(hMod, "WTSVirtualChannelOpenEx");
    return gpWTSVirtualChannelOpenEx != NULL;
}

DWORD OpenVirtualChannel(
    BOOL bDynamic,
    LPCSTR szChannelName,
    HANDLE* phWTSHandle,
    HANDLE* phFile)
{
    HANDLE hWTSHandle = NULL;
    HANDLE hWTSFileHandle;
    PVOID vcFileHandlePtr = NULL;
    DWORD len;
    DWORD rc = ERROR_SUCCESS;
    BOOL fSucc;

    if (bDynamic) {
        if (gpWTSVirtualChannelOpenEx == NULL)
        {
            rc = ERROR_PROC_NOT_FOUND;
            goto exitpt;
        }
        hWTSHandle = gpWTSVirtualChannelOpenEx(
            WTS_CURRENT_SESSION,
            (LPSTR)szChannelName,
            WTS_CHANNEL_OPTION_DYNAMIC | WTS_CHANNEL_OPTION_DYNAMIC_PRI_HIGH | WTS_CHANNEL_OPTION_DYNAMIC_NO_COMPRESS);
    }
    else {
        hWTSHandle = WTSVirtualChannelOpen(
            WTS_CURRENT_SERVER_HANDLE,
            WTS_CURRENT_SESSION,
            (LPSTR)szChannelName);
    }
    if (NULL == hWTSHandle)
    {
        rc = GetLastError();
        goto exitpt;
    }

    fSucc = WTSVirtualChannelQuery(
        hWTSHandle,
        WTSVirtualFileHandle,
        &vcFileHandlePtr,
        &len);
    if (!fSucc)
    {
        rc = GetLastError();
        goto exitpt;
    }
    if (len != sizeof(HANDLE))
    {
        rc = ERROR_INVALID_PARAMETER;
        goto exitpt;
    }

    hWTSFileHandle = *(HANDLE*)vcFileHandlePtr;

    fSucc = DuplicateHandle(
        GetCurrentProcess(),
        hWTSFileHandle,
        GetCurrentProcess(),
        phFile,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS);
    if (!fSucc)
    {
        rc = GetLastError();
        goto exitpt;
    }

    rc = ERROR_SUCCESS;
    if (phWTSHandle) {
        *phWTSHandle = hWTSHandle;
        hWTSHandle = NULL;
    }

exitpt:
    if (vcFileHandlePtr)
    {
        WTSFreeMemory(vcFileHandlePtr);
    }
    if (hWTSHandle)
    {
        WTSVirtualChannelClose(hWTSHandle);
    }

    return rc;
}

class CTSChannelHandleToSocket
{
    HANDLE m_hWTSHandle;
    CPipe  m_tshandle;
    SOCKET m_hSocketA, m_hSocketB;
    HANDLE m_hThreadA, m_hThreadB;
    BOOL m_defFlagExit;
    BOOL* m_pFlagExit;
    Cnbsocket* m_sA;
    Cnbsocket* m_sB;
    CInetAbortorBase m_ab;
    HANDLE m_hExitEvent;
public:
    CTSChannelHandleToSocket() {
        m_hWTSHandle = NULL;
        m_hSocketA = m_hSocketB = INVALID_SOCKET;
        m_hThreadA = m_hThreadB = NULL;
        m_defFlagExit = FALSE;
        m_pFlagExit = &m_defFlagExit;
        m_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        m_ab._pflag = m_pFlagExit;
        m_sA = Cnbsocket::createInstance();
        m_sA->SetAborter(&m_ab);
        m_sB = Cnbsocket::createInstance();
        m_sB->SetAborter(&m_ab);
    }
    ~CTSChannelHandleToSocket() {
        Close();
    }

    BOOL OpenChannel(LPCSTR lpszName, BOOL bDVC) {
        HANDLE hFile;
        HANDLE hChannelHandle;
        if (ERROR_SUCCESS != OpenVirtualChannel(
            bDVC,
            lpszName,
            &hChannelHandle,
            &hFile
        )) {
            return FALSE;
        }
        if (!socket_mkpipes(&m_hSocketA, &m_hSocketB, true)) {
            WTSVirtualChannelClose(hChannelHandle);
            CloseHandle(hFile);
            return FALSE;
        }
        m_hWTSHandle = hChannelHandle;
        m_tshandle.Attach(hFile);
        m_sA->Attach(m_hSocketA);
        m_sB->Attach(m_hSocketB);
        socket_setbufsize(m_hSocketA, 1024 * 1024 * 16);
        socket_setbufsize(m_hSocketB, 1024 * 1024 * 16);
        m_hThreadA = CreateThread(0, 0, sThreadA, this, 0, 0);
        m_hThreadB = CreateThread(0, 0, sThreadB, this, 0, 0);
        return TRUE;
    }

    Cnbsocket* GetSocket() {
        return m_sB;
    }
    void SetFlagExit(BOOL *p) {
        if (p) {
            m_pFlagExit = p;
        }
        else {
            m_pFlagExit = &m_defFlagExit;
        }
        m_ab._pflag = m_pFlagExit;
    }
    HANDLE GetExitEventHandle() {
        return m_hExitEvent;
    }

    void Close()
    {
        *m_pFlagExit = TRUE;
        if (m_hThreadA) {
            WaitForSingleObject(m_hThreadA, INFINITE);
            CloseHandle(m_hThreadA);
            m_hThreadA = NULL;
        }
        if (m_hThreadB) {
            WaitForSingleObject(m_hThreadB, INFINITE);
            CloseHandle(m_hThreadB);
            m_hThreadB = NULL;
        }
        if (m_hSocketA != INVALID_SOCKET) {
            closesocket(m_hSocketA);
            m_hSocketA = INVALID_SOCKET;
        }
        if (m_hSocketB != INVALID_SOCKET) {
            closesocket(m_hSocketB);
            m_hSocketB = INVALID_SOCKET;
        }
        m_tshandle.Close();
        if (m_sA) {
            m_sA->Detach();
            m_sA->Dereference();
            m_sA = NULL;
        }
        if (m_sB) {
            m_sB->Detach();
            m_sB->Dereference();
            m_sB = NULL;
        }
        if (m_hExitEvent) {
            CloseHandle(m_hExitEvent);
            m_hExitEvent = NULL;
        }
        if (m_hWTSHandle) {
            WTSVirtualChannelClose(m_hWTSHandle);
            m_hWTSHandle = NULL;
        }
    }

    static DWORD WINAPI sThreadA(void* param) {
        CTSChannelHandleToSocket* _this = (CTSChannelHandleToSocket*)param;
        return _this->threadA();
    }
    static DWORD WINAPI sThreadB(void* param) {
        CTSChannelHandleToSocket* _this = (CTSChannelHandleToSocket*)param;
        return _this->threadB();
    }

    DWORD threadA() {
        int ret;
        enum {
            bufsize = CHANNEL_PDU_LENGTH
        };
        char* buf = new char[bufsize];
        CHANNEL_PDU_HEADER* pHdr = (CHANNEL_PDU_HEADER*)buf;
        while (!*m_pFlagExit)
        {
            ret = m_tshandle.Read(buf, bufsize, 1000);
            if (ret < 0) {
                if (ret == CPipe::TIMEOUT)
                    continue;
                break;
            }
            else if (ret == 0) {
                break;
            }

            if (ret <= sizeof(CHANNEL_PDU_HEADER)) {
                break;
            }
            ret -= sizeof(CHANNEL_PDU_HEADER);
            ret = InetWriteAll(m_sA, pHdr+1, ret, 120);
            if (!ret) {
                break;
            }
        }
        delete[]buf;
        *m_pFlagExit = TRUE;
        m_sA->Shutdown(Inet::INET_SD_RDWR);
        SetEvent(m_hExitEvent);
        return 0;
    }

    DWORD threadB() {
        int ret;
        enum {
            bufsize = 1024 * 64
        };
        char* buf = new char[bufsize];
        while (!*m_pFlagExit)
        {
            ret = m_sA->Read(buf, bufsize, 1);
            if (ret < 0) {
                if (ret == Inet::E_INET_WOULDBLOCK)
                    continue;
                break;
            }
            else if (ret == 0) {
                break;
            }

            if (!PipeWriteAll(&m_tshandle, buf, ret, 120 * 1000)) {
                break;
            }
        }
        delete[]buf;
        *m_pFlagExit = TRUE;
        SetEvent(m_hExitEvent);
        return 0;
    }
    bool PipeWriteAll(CPipe *pipe, const void* data, int len, DWORD timeout_ms)
    {
        int ret;
        int blocksz;
        while (len > 0) {
            blocksz = min(len, CHANNEL_CHUNK_LENGTH);
            ret = pipe->Write(data, blocksz, timeout_ms);
            if (ret <= 0) {
                return false;
            }
            data = (char*)data + ret;
            len -= ret;
        }
        return true;
    }
};

#define TSCH2S(x) ((CTSChannelHandleToSocket*)x)

CTSChannelSocketWrapper::CTSChannelSocketWrapper()
{
    m_priv = NULL;
    m_psocket = NULL;
}

CTSChannelSocketWrapper::~CTSChannelSocketWrapper()
{
    Close();
}

BOOL CTSChannelSocketWrapper::Open(LPCSTR lpszName, BOOL bDVC)
{
    CTSChannelHandleToSocket* priv = new CTSChannelHandleToSocket();
    if (!priv->OpenChannel(lpszName, bDVC)) {
        delete priv;
        return FALSE;
    }
    m_priv = priv;
    m_psocket = priv->GetSocket();
    return TRUE;
}

void CTSChannelSocketWrapper::Close()
{
    if (m_priv) {
        TSCH2S(m_priv)->Close();
        delete TSCH2S(m_priv);
        m_priv = NULL;
    }
}

HANDLE CTSChannelSocketWrapper::GetExitEventHandle()
{
    return TSCH2S(m_priv)->GetExitEventHandle();
}

void CTSChannelSocketWrapper::SetFlagExit(BOOL* p)
{
    return TSCH2S(m_priv)->SetFlagExit(p);
}

bool CTSChannelSocketWrapper::WriteAll(const void* data, int len, int timeout_sec)
{
    bool ret = InetWriteAll(m_psocket, data, len, timeout_sec);
    if (ret) {
        m_totalWritten += len;
    }
    return ret;
}

int CTSChannelSocketWrapper::Read(void* buf, int size, int timeout_sec)
{
    int ret = m_psocket->Read(buf, size, timeout_sec);
    if (ret > 0) {
        m_totalRead += ret;
    }
    return ret;
}

void CTSChannelSocketWrapper::GetStat(UINT64* W, UINT64* R)
{
    if (W)
        *W = m_totalWritten;
    if (R)
        *R = m_totalRead;
}
