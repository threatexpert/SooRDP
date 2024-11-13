// SooRDPServerApp.cpp : 定义应用程序的入口点。
//

#include "pch.h"
#include "SooRDPServerApp.h"
#include "TSChannel.h"
#include "../misc/NBSocket.h"
#include "../misc/netutils.h"
#include "../misc/Thread.h"
#include "../misc/human-readable.h"
#include "../misc/SoTunnel.h"
#include "../SooRDP-Plugin/idata.h"
#include <list>
#include <string>
#include "langstr.h"

// 全局变量:
HINSTANCE hInst;                                // 当前实例
HWND hMainDlg;

int gMode = rdccmode_conn;
char gConnectIP[128] = "127.0.0.1";
int gConnectPort = 9000;
HANDLE ghThreadTunnelMaker = NULL;
BOOL gFlagStop = FALSE;
int idinc = 0;
WCHAR gTmpBuf[1024];
struct tunnelctx* gtctx = NULL;

#define TM_CHKSTOP 3000
#define TM_CHANNELSTAT 3001
#define MYWM_MSG  WM_USER+1
#define MYWM_TUNNELINS  WM_USER+2
#define MYWM_CHANNELSTAT  WM_USER+3
#define MYWM_DATATUNNEL_END  WM_USER+4

void CenterWindow(HWND hDlg)
{
    HWND hwndOwner = NULL;
    RECT rcOwner, rcDlg, rc;
    if ((hwndOwner = GetParent(hDlg)) == NULL)
    {
        hwndOwner = GetDesktopWindow();
    }
    GetWindowRect(hwndOwner, &rcOwner);
    GetWindowRect(hDlg, &rcDlg);
    CopyRect(&rc, &rcOwner);
    OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
    OffsetRect(&rc, -rc.left, -rc.top);
    OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
    SetWindowPos(hDlg,
        HWND_TOP,
        rcOwner.left + (rc.right / 2),
        rcOwner.top + (rc.bottom / 2),
        0, 0,
        SWP_NOSIZE);
}

void UpdateStatusText(LPCTSTR fmt, ...)
{
    WCHAR buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(buf, _countof(buf)-3, fmt, ap);
    va_end(ap);
    _tcscat_s(buf, _T("\r\n"));
    PostMessage(hMainDlg, MYWM_MSG, 0, (LPARAM)_wcsdup(buf));
}

class CSoTunnelRemote
    : public CSoTunnel
{
    CasTcpPair* m_pST, * m_pChn;
    HANDLE m_hExitEvent;
public:
    int tunnelid;
    int m_conn_count;
    CSoTunnelRemote() {
        tunnelid = 0;
        m_pST = m_pChn = NULL;
        m_hExitEvent = CreateEvent(0, 1, 0, 0);
        m_conn_count = 0;
    }
    ~CSoTunnelRemote() {
        delete m_pST;
        delete m_pChn;
        CloseHandle(m_hExitEvent);
    }

    BOOL Init(SOCKET sSVCChannel) {
        m_pST = new CasTcpPair();
        m_pChn = new CasTcpPair();
        m_pST->m_pp = m_pChn;
        m_pChn->m_pp = m_pST;

        m_pChn->m_hSocket = sSVCChannel;
        EnableKeepalive(20);
        if (!InitRemote()) {
            SetEvent(m_hExitEvent);
            return FALSE;
        }

        m_pST->m_hSocket = m_pTunnelIO->Detach();

        if (!AddASocket(m_pChn, FD_READ | FD_WRITE | FD_CLOSE)) {
            m_lasterr = L"AddASocket m_pChn Failed";
            SetEvent(m_hExitEvent);
            return FALSE;
        }
        if (!AddASocket(m_pST, FD_READ | FD_WRITE | FD_CLOSE)) {
            m_lasterr = L"AddASocket m_pST Failed";
            SetEvent(m_hExitEvent);
            return FALSE;
        }
        return TRUE;
    }

    HANDLE GetExitEventHandle()
    {
        return m_hExitEvent;
    }

    virtual void OnChannelSocketReady(bool ok) {
        if (!ok) {
            UpdateStatusText(LSTRW(RID_NotReady));
            SendReset();
        }
    }
    virtual void OnChannelSocketClose()
    {
        UpdateStatusText(LSTRW(RID_TunnelSocketClosed));
        SetEvent(m_hExitEvent);
    }
    virtual void OnChannelConfigChanged(st_config* config) {
        if (config->mode == rdccmode_conn) {
            UpdateStatusText(LSTRW(RID_TunnelModeChanged_mode_fwd), tunnelid, config->ip, (int)config->port);
        }
        else if (config->mode == rdccmode_s5) {
            UpdateStatusText(LSTRW(RID_TunnelModeChanged_mode_s5), tunnelid);
        }
        else {
            UpdateStatusText(TEXT("createing RDP tunnel#% error, Unk mode"), tunnelid);
            SetEvent(m_hExitEvent);
        }
    };

    virtual void OnChannelSocketCount(int count) {
        m_conn_count = count;
    };
    virtual void OnChannelSocks5ConnectTarget(const char* host, int port) {
        UpdateStatusText(LSTRW(RID_Tunnel_S5_connect_to), host, port);
    };
    virtual void OnChannelPingTimeout() {
        UpdateStatusText(LSTRW(RID_Tunnel_Ping_timeout), tunnelid);
        SetEvent(m_hExitEvent);
    }

    uint64_t GetStatRead() {
        if (m_pChn) return m_pChn->m_total_read;
        return 0;
    }
    uint64_t GetStatWritten() {
        if (m_pChn) return m_pChn->m_total_written;
        return 0;
    }

};

struct tunnelctx {
    int id;
    BOOL flagExit;
    CTSChannelSocketWrapper tsc;
    CSoTunnelRemote sot;
};

int soordp_srv(tunnelctx &tunnel) {
    int			ret;
    
    BOOL bDVC = supportDynamicVirtualChannel();
    //bDVC = FALSE;
    if (bDVC) {
        UpdateStatusText(LSTRW(RID_Creating_DVC), tunnel.id);
    }
    else {
        UpdateStatusText(LSTRW(RID_Creating_SVC), tunnel.id);
    }
    if (!tunnel.tsc.Open(bDVC ? "SOORDPD" : "SOORDPS", bDVC)) {
        UpdateStatusText(LSTRW(RID_Creating_Tunnel_Failed), tunnel.id, GetLastError());
        return -1;
    }
    tunnel.tsc.SetFlagExit(&tunnel.flagExit);
    tunnel.sot.tunnelid = tunnel.id;
    ret = tunnel.sot.Init(tunnel.tsc.m_psocket->Detach());
    if (!ret) {
        UpdateStatusText(LSTRW(RID_Creating_FWD_Failed), tunnel.id, tunnel.sot.m_lasterr.c_str());
        return -1;
    }

    SendMessage(hMainDlg, MYWM_CHANNELSTAT, 1, 0);
    HANDLE hexit[2] = { tunnel.tsc.GetExitEventHandle(), tunnel.sot.GetExitEventHandle() };
    while (!gFlagStop) {
        if (WAIT_TIMEOUT == WaitForMultipleObjects(2, hexit, FALSE, 1000)) {
            continue;
        }
        break;
    }
    UpdateStatusText(LSTRW(RID_Tunnel_End), tunnel.id);
    tunnel.tsc.Close();
    tunnel.sot.Deinit();
    SendMessage(hMainDlg, MYWM_CHANNELSTAT, 0, 0);
    return 0;
}

DWORD WINAPI TunnelMaker(void*)
{
    DWORD delay = 0;
    int t = 0;
    while (!gFlagStop)
    {
        Sleep(delay);
        if (gFlagStop)
            break;
        tunnelctx tunnel;
        tunnel.id = t++;
        tunnel.flagExit = TRUE;
        SendMessage(hMainDlg, MYWM_TUNNELINS, 0, (LPARAM)&tunnel);
        if (tunnel.flagExit)
            break;
        if (soordp_srv(tunnel) < 0) {
            delay = 5000;
        }
        else {
            delay = 1000;
        }
        SendMessage(hMainDlg, MYWM_TUNNELINS, 0, (LPARAM)NULL);
    }
    return 0;
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CenterWindow(hDlg);
        
        swprintf_s(gTmpBuf, L"%s - v2.1", LSTRW(RID_Title));

        SetWindowText(hDlg, gTmpBuf);
        SendMessage(GetDlgItem(hDlg, IDC_EDIT_LOG), EM_SETLIMITTEXT, 0, 0);
        SetDlgItemText(hDlg, IDOK, LSTRW(RID_Start));
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            if (ghThreadTunnelMaker == NULL)
            {
                gFlagStop = FALSE;
                ghThreadTunnelMaker = CreateThread(0, 0, TunnelMaker, 0, 0, NULL);
                SetDlgItemText(hDlg, IDOK, LSTRW(RID_Stop));
            }
            else
            {
                gFlagStop = TRUE;
                if (gtctx) {
                    gtctx->flagExit = TRUE;
                    gtctx = NULL;
                }
                SetDlgItemText(hDlg, IDOK, LSTRW(RID_Stopping));
                SetTimer(hDlg, TM_CHKSTOP, 1000, NULL);
            }
        }
        break;
        case IDCANCEL:
        case IDM_EXIT:
            SendMessage(hDlg, WM_CLOSE, 0, 0);
            return TRUE;
        }
        break;

    case MYWM_TUNNELINS:
    {
        gtctx = (tunnelctx*)lParam;
        if (gtctx) {
            gtctx->flagExit = FALSE;
        }
    }
    break;
    case MYWM_CHANNELSTAT:
    {
        if (wParam) {
            SetTimer(hDlg, TM_CHANNELSTAT, 1000, NULL);
        }
        else {
            KillTimer(hDlg, TM_CHANNELSTAT);
        }
    }
    break;
    case MYWM_MSG:
    {
        HWND h = GetDlgItem(hDlg, IDC_EDIT_LOG);
        int nLength = GetWindowTextLength(h);
        enum {
            maxlogsize = 1024 * 1024 * 2
        };
        if (nLength > maxlogsize) {
            ::SendMessageW(h, EM_SETSEL, 0, nLength /2);
            ::SendMessageW(h, EM_REPLACESEL, (WPARAM)0, (LPARAM)L"");
        }
        nLength = GetWindowTextLength(h);
        ::SendMessageW(h, EM_SETSEL, nLength, nLength);
        ::SendMessageW(h, EM_SCROLLCARET, 0, 0);
        ::SendMessageW(h, EM_REPLACESEL, (WPARAM)0, (LPARAM)lParam);
        free((WCHAR*)lParam);
    }
    break;
    case MYWM_DATATUNNEL_END:
    {

    }
    break;

    case WM_TIMER:
    {
        switch (wParam) {
        case TM_CHKSTOP:
        {
            if (WAIT_OBJECT_0 == WaitForSingleObject(ghThreadTunnelMaker, 0)) {
                CloseHandle(ghThreadTunnelMaker);
                ghThreadTunnelMaker = NULL;
                SetDlgItemText(hDlg, IDOK, LSTRW(RID_Start));
                SetDlgItemText(hDlg, IDC_STATIC_MAIN, TEXT(": )"));
                KillTimer(hDlg, TM_CHKSTOP);
            }
        }
        break;
        case TM_CHANNELSTAT:
        {
            if (gtctx) {
                CHAR szW[100], szR[100];
                WCHAR text[1024];

                calculateSize1024(gtctx->sot.GetStatWritten(), szW, 100);
                calculateSize1024(gtctx->sot.GetStatRead(), szR, 100);

                swprintf_s(text, LSTRW(RID_Tunnel_Status), gtctx->id, szW, szR, (int)gtctx->sot.m_conn_count);
                SetDlgItemText(hDlg, IDC_STATIC_MAIN, text);
            }
        }
        break;
        }
    }
    break;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;
    }

    return FALSE;
}

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE h0, LPTSTR lpCmdLine, int nCmdShow)
{
    HWND hDlg;
    MSG msg;
    BOOL ret;
    ::hInst = hInst;

    if (GetUserDefaultUILanguage() != MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) &&
        GetUserDefaultUILanguage() != MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) {
        SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    }

    socket_init_ws32();
    InitCommonControls();
    hMainDlg = hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG1), 0, DialogProc, 0);
    ShowWindow(hDlg, nCmdShow);

    while ((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
        if (ret == -1)
            return -1;

        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    gFlagStop = TRUE;
    if (gtctx) {
        gtctx->flagExit = TRUE;
        gtctx = NULL;
    }
    if (WAIT_OBJECT_0 == WaitForSingleObject(ghThreadTunnelMaker, 10000)) {
        CloseHandle(ghThreadTunnelMaker);
        ghThreadTunnelMaker = NULL;
    }
    else {
        ExitProcess(7);
    }
    return 0;
}

