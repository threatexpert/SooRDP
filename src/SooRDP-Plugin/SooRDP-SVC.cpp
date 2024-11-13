#include "pch.h"
#include "SooRDP-SVC.h"
#include <cchannel.h>
#include <map>
#include <list>
#include "../misc/memshare.h"
#include "../misc/netutils.h"
#include "../misc/Thread.h"
#include "../misc/SoTunnel.h"
#include "idata.h"

PCHANNEL_ENTRY_POINTS gpEntryPoints;
LPHANDLE gphChannel;
extern _SharedMem* gshmconfig;
extern Cmemshare g_shm;
class CTSChannelToSocket* gpT2S = NULL;
class CSoTunnel4SVC* gpSoTunnel = NULL;
extern void WINAPI VirtualChannelOpenEventX(DWORD openHandle, UINT event,
    LPVOID pdata, UINT32 dataLength,
    UINT32 totalLength, UINT32 dataFlags);
HANDLE ghThreadSetupTunnel;
BOOL gFlagExitSetupTunnel;

class VCOutputBlock {
public:
    void* ctx;
    uint64_t key;
    int len;
    char data[1];

    static VCOutputBlock* create(int datalen) {
        int sz = sizeof(VCOutputBlock) - 1 + datalen;
        return (VCOutputBlock*)malloc(sz);
    }
};

class CTSChannelToSocket
{
    HANDLE m_hThreadA;
    BOOL m_FlagExit;
    Cnbsocket* m_sA;
    Cnbsocket* m_sB;
    CInetAbortorBase m_ab;
    CMyCriticalSection m_lc_ob;
    typedef std::list<VCOutputBlock*> VCOBHistory;
    VCOBHistory m_output_blocks;
    uint64_t m_output_block_id_autoinc;
    uint64_t m_all_pending_output_blocksize;
    HANDLE m_write_sync;
    BOOL m_sent_EOF;
public:
    DWORD m_openHandle;
    CTSChannelToSocket() {
        m_hThreadA  = NULL;
        m_FlagExit = FALSE;
        m_ab._pflag = &m_FlagExit;
        m_sA = Cnbsocket::createInstance();
        m_sA->SetAborter(&m_ab);
        m_sB = Cnbsocket::createInstance();
        m_sB->SetAborter(&m_ab);
        m_output_block_id_autoinc = 0;
        m_all_pending_output_blocksize = 0;
        m_openHandle = 0;
        m_write_sync = CreateEvent(0, 1, 0, 0);
        m_sent_EOF = FALSE;
    }
    ~CTSChannelToSocket() {
        Close();
    }

    BOOL Init(DWORD openHandle) {
        
        m_openHandle = openHandle;
        SOCKET sA, sB;
        if (!socket_mkpipes(&sA, &sB, true)) {
            return FALSE;
        }

        m_sA->Attach(sA);
        m_sB->Attach(sB);
        socket_setbufsize(sA, ChnSocketBufferSize);
        socket_setbufsize(sB, ChnSocketBufferSize);
        m_hThreadA = CreateThread(0, 0, sThreadA, this, 0, 0);
        return TRUE;
    }

    Cnbsocket* GetSocket() {
        return m_sB;
    }
    void SetFlagExit() {
        m_FlagExit = TRUE;
    }
    HANDLE GetExitEventHandle() {
        return m_hThreadA;
    }

    void Close()
    {
        m_FlagExit = TRUE;
        if (m_hThreadA) {
            WaitForSingleObject(m_hThreadA, INFINITE);
            CloseHandle(m_hThreadA);
            m_hThreadA = NULL;
        }

        if (m_sA) {
            m_sA->Dereference();
            m_sA = NULL;
        }
        if (m_sB) {
            m_sB->Dereference();
            m_sB = NULL;
        }
        if (m_write_sync) {
            CloseHandle(m_write_sync);
            m_write_sync = NULL;
        }

        if (m_openHandle != 0) {
            gpEntryPoints->pVirtualChannelClose(m_openHandle);
            m_openHandle = 0;
        }
    }

    static DWORD WINAPI sThreadA(void* param) {
        CTSChannelToSocket* _this = (CTSChannelToSocket*)param;
        return _this->threadA();
    }

    DWORD threadA() {
        int ret;
        enum {
            bufsize = 1024 * 64
        };
        char* buf = new char[bufsize];
        while (!m_FlagExit)
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

            if (!OutputVCData(buf, ret)) {
                break;
            }
        }
        delete[]buf;
        return 0;
    }

    BOOL InputVCData(const void* data, int len) {
        return InetWriteAll(m_sA, data, len, 120);
    }

    BOOL WaitPendingOutput() {
        while (!m_FlagExit) {
            BOOL cond = FALSE;
            {
                CAutoCriticalSection lc(m_lc_ob);
                if (m_all_pending_output_blocksize > 1024 * 1024 * 50) {
                    ResetEvent(m_write_sync);
                    cond = TRUE;
                }
            }
            if (!cond) {
                return TRUE;
            }
            WaitForSingleObject(m_write_sync, 1000);
        }
        return FALSE;
    }

    BOOL OutputVCData(const void* data, int len) {

        if (!WaitPendingOutput()) {
            return FALSE;
        }

        CAutoCriticalSection lc(m_lc_ob);

        VCOutputBlock* ob = VCOutputBlock::create(len);
        if (!ob)
            return FALSE;

        ob->ctx = this;
        ob->key = m_output_block_id_autoinc++;
        ob->len = len;
        memcpy(ob->data, data, len);
        m_output_blocks.push_back(ob);
        m_all_pending_output_blocksize += len;
        ResetEvent(m_write_sync);
        if (CHANNEL_RC_OK != gpEntryPoints->pVirtualChannelWrite(m_openHandle, ob->data, ob->len, ob)) {
            m_output_blocks.pop_back();
            m_all_pending_output_blocksize -= len;
            free(ob);
            return FALSE;
        }
        return TRUE;
    }
    void OnOutputVCDataComplete(VCOutputBlock* ob) {
        CAutoCriticalSection lc(m_lc_ob);

        m_all_pending_output_blocksize -= ob->len;
        VCOutputBlock* p = *m_output_blocks.begin();
        if (p == ob) {
            m_output_blocks.pop_front();
            free(ob);
            SetEvent(m_write_sync);
            return;
        }
        m_output_blocks.remove(ob);
        free(ob);
        SetEvent(m_write_sync);
    }
    static void OnOutputVCDataComplete(void* userData) {
        if (!userData) {
            return;
        }
        VCOutputBlock* ob = (VCOutputBlock*)userData;
        CTSChannelToSocket* _this = (CTSChannelToSocket*)ob->ctx;
        _this->OnOutputVCDataComplete(ob);
    }
};

class CSoTunnel4SVC
    : public CSoTunnel
{
    CasTcpPair* m_pST, *m_pChn;
    HANDLE m_hExitEvent;
public:
    CSoTunnel4SVC()
    {
        m_pST = m_pChn = NULL;
        m_hExitEvent = CreateEvent(0, 1, 0, 0);
    }
    ~CSoTunnel4SVC()
    {
        CSoTunnel::Deinit();
        delete m_pST;
        delete m_pChn;
        CloseHandle(m_hExitEvent);
    }

    HANDLE GetExitEventHandle()
    {
        return m_hExitEvent;
    }
    
    BOOL Init(int listenPort, SOCKET sSVCChannel) {
        m_pST = new CasTcpPair();
        m_pChn = new CasTcpPair();
        m_pST->m_pp = m_pChn;
        m_pChn->m_pp = m_pST;

        m_pChn->m_hSocket = sSVCChannel;
        EnableKeepalive(20);
        if (!InitLocal(listenPort)) {
            return FALSE;
        }
        m_pST->m_hSocket = m_pTunnelIO->Detach();

        if (!AddASocket(m_pChn, FD_READ | FD_WRITE | FD_CLOSE)) {
            return FALSE;
        }
        if (!AddASocket(m_pST, FD_READ | FD_WRITE | FD_CLOSE)) {
            return FALSE;
        }
        return TRUE;
    }


    virtual void OnChannelSocketReady(bool ok) {
        if (!ok) {
            sendLog(LSTRW(RID_TunnelNotReady));
            SendReset();
        }
        else {
            sendLog(LSTRW(RID_TunnelReady));
            SendConfig(gshmconfig->mode, gshmconfig->targetip, gshmconfig->targetport);
        }
    }
    virtual void OnChannelSocketClose()
    {
        sendLog(LSTRW(RID_SVCSocketClosed));
        SetEvent(m_hExitEvent);
    }
    virtual void OnChannelPingTimeout() {
        sendLog(LSTRW(RID_SVCTimeoutClose));
        SendReset();
    };


};

DWORD WINAPI thread_SetupTunnel(void* param)
{
    DWORD openHandle;
    UINT ui = 0;
    BOOL first_open = TRUE;
    for (;!gFlagExitSetupTunnel;)
    {
        sendLog(LSTRW(RID_LocallyOpenSVCTunnel));
        ui = gpEntryPoints->pVirtualChannelOpen(gphChannel,
            &openHandle,
            "SOORDPS",
            (PCHANNEL_OPEN_EVENT_FN)VirtualChannelOpenEventX);

        if (ui != CHANNEL_RC_OK) {
            sendLog(LSTRW(RID_LocallyOpenSVCTunnelFailed), ui);
            Sleep(2000);
            continue;
        }

        gpT2S = new CTSChannelToSocket();
        if (!gpT2S->Init(openHandle)) {
            delete gpT2S;
            gpT2S = NULL;
            Sleep(2000);
            continue;
        }
        gpSoTunnel = new CSoTunnel4SVC();
        if (!gpSoTunnel->Init(gshmconfig->port, gpT2S->GetSocket()->Detach())) {
            ATLTRACE("VirtualChannelInitEventProc ERROR gpSoTunnel->InitLocal\n");
            sendLog(LSTRW(RID_Tunnel_Init_Err), gpSoTunnel->m_lasterr.c_str());
            delete gpT2S;
            gpT2S = NULL;
            delete gpSoTunnel;
            gpSoTunnel = NULL;
            Sleep(2000);
            continue;
        }

        if (!first_open) {
            gpSoTunnel->SendRUReady();
        }
        first_open = FALSE;

        sendLog(LSTRW(RID_Tunnel_Init_Done), (int)gshmconfig->port);

        HANDLE hexit[2] = { gpT2S->GetExitEventHandle() , gpSoTunnel->GetExitEventHandle() };
        while (!gFlagExitSetupTunnel) {
            if (WAIT_TIMEOUT == WaitForMultipleObjects(2, hexit, FALSE, 1000)) {
                continue;
            }
            break;
        }

        sendLog(LSTRW(RID_TunnelErr_closing));
        if (gpT2S) {
            delete gpT2S;
            gpT2S = NULL;
        }
        if (gpSoTunnel) {
            delete gpSoTunnel;
            gpSoTunnel = NULL;
        }
        Sleep(2000);
    }

    return 0;
}

void CloseTunnel_SVC()
{
    gFlagExitSetupTunnel = TRUE;
    if (ghThreadSetupTunnel) {
        WaitForSingleObject(ghThreadSetupTunnel, INFINITE);
        CloseHandle(ghThreadSetupTunnel);
        ghThreadSetupTunnel = NULL;
    }
    if (gpT2S) {
        delete gpT2S;
        gpT2S = NULL;
    }
    if (gpSoTunnel) {
        delete gpSoTunnel;
        gpSoTunnel = NULL;
    }
}

void SetupTunnel_SVC()
{
    CloseTunnel_SVC();
    gFlagExitSetupTunnel = FALSE;
    ghThreadSetupTunnel = CreateThread(0, 0, thread_SetupTunnel, 0, 0, 0);
}

void WINAPI VirtualChannelOpenEventX(DWORD openHandle, UINT event,
    LPVOID pdata, UINT32 dataLength,
    UINT32 totalLength, UINT32 dataFlags)
{
    //ATLTRACE("VirtualChannelOpenEventX openHandle=%d, event=%d, dataLength=%d, totalLength=%d\n", openHandle, event, dataLength, totalLength);
    LPDWORD pdwControlCode = (LPDWORD)pdata;
    UINT ui = 0;

    UNREFERENCED_PARAMETER(dataFlags);

    if (gpT2S == NULL) {
        ATLTRACE("VirtualChannelOpenEventX ERROR gpT2S == NULL!!!!!!!!!!!!!!!!!!!!!!!!!! \n");
        return;
    }
    if (gpT2S->m_openHandle != openHandle) {
        ATLTRACE("VirtualChannelOpenEventX ERROR wrong openHandle !!!!!!!!!!!!!!!!!!!!!!!!!! \n");
        CloseTunnel_SVC();
        return;
    }


    switch (event) {
    case CHANNEL_EVENT_DATA_RECEIVED: {
        if (dataLength == 0 && totalLength == 0) {
            CloseTunnel_SVC();
            return;
        }
        if (!gpT2S->InputVCData(pdata, dataLength)) {
            ATLTRACE("VirtualChannelOpenEventX ERROR InputVCData !!!!!!!!!!!!!!!!!!!!!!!!!! \n");
            CloseTunnel_SVC();
        }
    }break;
    case CHANNEL_EVENT_WRITE_COMPLETE: {
        CTSChannelToSocket::OnOutputVCDataComplete(pdata);
    }break;
    case CHANNEL_EVENT_WRITE_CANCELLED: {}break;
    default: {}break;
    }
}

VOID VCAPITYPE VirtualChannelInitEventProc(LPVOID pInitHandle, UINT event,
    LPVOID pData, UINT dataLength)
{
    ATLTRACE("VirtualChannelInitEventProc event=%d, dataLength=%d\n", event, dataLength);

    UNREFERENCED_PARAMETER(pInitHandle);
    UNREFERENCED_PARAMETER(dataLength);

    switch (event) {
    case CHANNEL_EVENT_INITIALIZED: {
    
    }break;

    case CHANNEL_EVENT_CONNECTED: {
        sendLog(LSTRW(RID_AskingMakeSVCTunnel));
        SetupTunnel_SVC();
    }break;

    case CHANNEL_EVENT_V1_CONNECTED: {
    }break;

    case CHANNEL_EVENT_DISCONNECTED: {
        ATLTRACE("VirtualChannelInitEventProc event=CHANNEL_EVENT_DISCONNECTED\n");
        //sendLog(_T("远程桌面已断开."));
        CloseTunnel_SVC();
    }break;

    case CHANNEL_EVENT_TERMINATED: {
        //sendLog(_T("远程桌面已终止."));
        LocalFree((HLOCAL)gpEntryPoints);
    }break;

    default: {}break;
    }
}

BOOL VCAPITYPE VirtualChannelEntry(
    PCHANNEL_ENTRY_POINTS pEntryPoints
)
{
    ATLTRACE("VirtualChannelEntry\n");
    CHANNEL_DEF cd;
    UINT uRet;

    char var[128] = "";
    GetEnvironmentVariableA(SharedMemConfig, var, _countof(var));
    if (!var[0]) {
        strcpy_s(var, SharedMemConfig);
    }

    if (g_shm.GetMem() == NULL) {
        if (!g_shm.Open(var, sizeof(_SharedMem))) {
            ATLTRACE("CTsClientX::FinalConstruct g_shm.Open failed\n");
            return FALSE;
        }
    }
    gshmconfig = (_SharedMem*)g_shm.GetMem();


    //
    // allocate memory
    //
    gpEntryPoints =
        (PCHANNEL_ENTRY_POINTS)LocalAlloc(LPTR, pEntryPoints->cbSize);

    memcpy(gpEntryPoints, pEntryPoints, pEntryPoints->cbSize);

    //
    // initialize CHANNEL_DEF structure
    //
    ZeroMemory(&cd, sizeof(cd));
    strcpy_s(cd.name, "SOORDPS");
    cd.options |= CHANNEL_OPTION_SHOW_PROTOCOL;

    //
    // register channel
    //
    uRet =
        gpEntryPoints->pVirtualChannelInit((LPVOID*)&gphChannel,
            &cd, 1,
            VIRTUAL_CHANNEL_VERSION_WIN2000,
            (PCHANNEL_INIT_EVENT_FN)
            VirtualChannelInitEventProc);

    if (uRet != CHANNEL_RC_OK) {
        return FALSE;
    }

    //
    // make sure channel was initialized
    //
    if (!(cd.options & CHANNEL_OPTION_INITIALIZED)) {
        return FALSE;
    }
    return TRUE;
}
