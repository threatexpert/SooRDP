
// SooRDPClientAppDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "SooRDPClientApp.h"
#include "SooRDPClientAppDlg.h"
#include "afxdialogex.h"
#include <sddl.h>
#include "../misc/ProcessUtils.h"
#include "langstr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TM_AUTORUN 3000
#define TM_UpdataCfg 3001
#define TM_CheckDeadMstsc 3002

#define const_MstscConnTimeout 15000

#define MYWM_NOTIFY WM_USER+100

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CSooRDPClientAppDlg 对话框



CSooRDPClientAppDlg::CSooRDPClientAppDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SOORDPCLIENTAPP_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_bInstalled = FALSE;
	m_hProcMstsc = NULL;
	m_dwPidMstsc = 0;
	m_bAutorunMstsc = FALSE;
	m_bUseMEDIUM_INTEGRITY = FALSE;
	m_pInsCount = NULL;
	m_bMstscConnected = FALSE;
}

void CSooRDPClientAppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_LOG, m_editLog);
	DDX_Control(pDX, IDC_EDIT_N, m_editNotify);
	DDX_Control(pDX, IDC_COMBO_MODE, m_cbMode);
}

BEGIN_MESSAGE_MAP(CSooRDPClientAppDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_CHECK_AUTORUN, &CSooRDPClientAppDlg::OnBnClickedCheckAutorun)
	ON_WM_TIMER()
	ON_MESSAGE(MYWM_NOTIFY, &CSooRDPClientAppDlg::OnMywmNotify)
	ON_EN_UPDATE(IDC_EDIT_N, &CSooRDPClientAppDlg::OnEnUpdateEditN)
	ON_STN_CLICKED(IDC_LOGO, &CSooRDPClientAppDlg::OnStnClickedLogo)
	ON_EN_UPDATE(IDC_EDIT_PORT, &CSooRDPClientAppDlg::OnEnUpdateConfig)
	ON_EN_UPDATE(IDC_EDIT_RADDR, &CSooRDPClientAppDlg::OnEnUpdateConfig)
	ON_EN_UPDATE(IDC_EDIT_RPORT, &CSooRDPClientAppDlg::OnEnUpdateConfig)
	ON_CBN_SELCHANGE(IDC_COMBO_MODE, &CSooRDPClientAppDlg::OnCbnSelchangeComboMode)
	ON_BN_CLICKED(ID_OPEN, &CSooRDPClientAppDlg::OnBnClickedOpen)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTON_CHOOSE_RDP, &CSooRDPClientAppDlg::OnBnClickedButtonChooseRdp)
END_MESSAGE_MAP()


bool shmtest(char *namebuf, int bufsize)
{
	Cmemshare shmtest;
	if (!shmtest.Open(SharedMemConfig, sizeof(_SharedMem))) {
		strcpy_s(namebuf, bufsize, SharedMemConfig);
		return true;
	}
	shmtest.Close();
	for (int i = 1; i < 100; i++) {
		sprintf_s(namebuf, bufsize, "%s%d", SharedMemConfig, i);
		if (shmtest.Open(namebuf, sizeof(_SharedMem))) {
			shmtest.Close();
		}
		else {
			return true;
		}
	}
	return false;
}

// CSooRDPClientAppDlg 消息处理程序

BOOL CSooRDPClientAppDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	CString strTitle;
	strTitle.Format(_T("%s v2.2 - 狼蛛安全实验室"), LSTRW(RID_TITLE));
	SetWindowText(strTitle);

	CString pluginPath;
	CStringA pluginPathA;
	CString strtmp;
	FindPlugin(pluginPath);
	pluginPath.Replace('\\', '_');
	pluginPath.Replace('_', '_');
	pluginPath.Replace(':', '_');
	pluginPathA = (LPCWSTR)pluginPath.MakeLower();
	if (!m_shm_InsCount.Open(pluginPathA, sizeof(DWORD))) {
		if (!m_shm_InsCount.Create(pluginPathA, sizeof(DWORD))) {
			strtmp.Format(_T("初始化共享内存异常0: %d"), GetLastError());
			MessageBox(strtmp, _T("提示"), MB_ICONERROR);
			exit(1);
		}
	}
	m_pInsCount = (DWORD*)m_shm_InsCount.GetMem();
	++(*m_pInsCount);

	char shmname[128] = SharedMemConfig;
	if (!shmtest(shmname, 128)
		|| !m_shm.Create(shmname, sizeof(_SharedMem))) {
		strtmp.Format(_T("初始化配置信息共享内存异常1: %d"), GetLastError());
		MessageBox(strtmp, _T("提示"), MB_ICONERROR);
		exit(1);
	}
	if (!SetEnvironmentVariableA(SharedMemConfig, shmname)) {
		strtmp.Format(_T("初始化配置信息共享内存异常2: %d"), GetLastError());
		MessageBox(strtmp, _T("提示"), MB_ICONERROR);
		exit(1);
	}

	TRACE("CSooRDPClientAppDlg::OnInitDialog shm=%s\n", shmname);

	m_pConfig = (_SharedMem*)m_shm.GetMem();
	m_pConfig->hWnd = m_hWnd;
	m_pConfig->uMsg = MYWM_NOTIFY;
	m_pConfig->port = 9999;
	m_pConfig->mode = rdccmode_conn;
	strcpy_s(m_pConfig->targetip, _countof(m_pConfig->targetip), "127.0.0.1");
	m_pConfig->targetport = 8000;
	m_pConfig->hText = m_editNotify.m_hWnd;
	m_editLog.SetLimitText(0);

	SetDlgItemInt(IDC_EDIT_PORT, m_pConfig->port);
	::SetDlgItemTextA(m_hWnd, IDC_EDIT_RADDR, m_pConfig->targetip);
	SetDlgItemInt(IDC_EDIT_RPORT, m_pConfig->targetport);
	m_cbMode.InsertString(0, LSTRW(RID_MODE_FORWARD));
	m_cbMode.InsertString(1, LSTRW(RID_MODE_SOCKS5));
	m_cbMode.SetCurSel(0);
	KillTimer(TM_UpdataCfg);
	SetDlgItemText(IDC_EDIT_MSTSC, _T("C:\\Windows\\System32\\mstsc.exe"));

	typedef
		BOOL WINAPI __ChangeWindowMessageFilterEx(HWND hwnd, UINT message, DWORD action, PCHANGEFILTERSTRUCT pChangeFilterStruct);
	__ChangeWindowMessageFilterEx* pChangeWindowMessageFilterEx = (__ChangeWindowMessageFilterEx*)
		GetProcAddress(GetModuleHandleA("user32"), "ChangeWindowMessageFilterEx");
	if (pChangeWindowMessageFilterEx) {
		pChangeWindowMessageFilterEx(m_hWnd, MYWM_NOTIFY, MSGFLT_ALLOW, NULL);
		pChangeWindowMessageFilterEx(m_editNotify.m_hWnd, WM_SETTEXT, MSGFLT_ALLOW, NULL);
	}

	m_bAutorunMstsc = IsDlgButtonChecked(IDC_CHECK_AUTORUN) == BST_CHECKED;

	DWORD dwPPid = 0;
	if (GetParentPid(GetCurrentProcessId(), &dwPPid, NULL, 0)) {
		IntegrityLevel plevel = GetProcessIntegrityLevel(dwPPid);
		IntegrityLevel mylevel = GetProcessIntegrityLevel(GetCurrentProcess());
		if (plevel == MEDIUM_INTEGRITY && mylevel > MEDIUM_INTEGRITY) {
			m_bUseMEDIUM_INTEGRITY = TRUE;
		}
	}
	TRACE("CSooRDPClientAppDlg:: m_bUseMEDIUM_INTEGRITY = %d\n", m_bUseMEDIUM_INTEGRITY);


#if _DEBUG
	Log(_T("调试版！"));
#endif

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CSooRDPClientAppDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CSooRDPClientAppDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CSooRDPClientAppDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CSooRDPClientAppDlg::OnBnClickedOpen()
{
	if (!m_bInstalled) {
		try
		{
			if (SUCCEEDED(RegisterPlugin(TRUE))) {
				m_bInstalled = TRUE;
			}
		}
		catch (CUserException* pe)
		{
			pe->Delete();
			return;
		}
		catch (CException* pe)
		{
			pe->ReportError();
			return;
		}
	}

	KillTimer(TM_UpdataCfg);
	UpdateConfig();

	if (!launch_mstsc()) {
		AfxMessageBox(LSTRW(RID_run_mstsc_err) + m_strError);
		return;
	}
	m_strtmp.Format(LSTRW(RID_mstsc_running), m_dwPidMstsc);
	Log(m_strtmp);
}

void CSooRDPClientAppDlg::OnDestroy()
{
	if (--(*m_pInsCount) == 0) {
		try
		{
			if (SUCCEEDED(RegisterPlugin(FALSE))) {

			}
		}
		catch (CUserException* pe)
		{
			pe->Delete();
			return;
		}
		catch (CException* pe)
		{
			pe->ReportError();
			return;
		}
	}

	if (m_hProcMstsc) {
		CloseHandle(m_hProcMstsc);
		m_hProcMstsc = NULL;
	}

	CDialogEx::OnDestroy();
}


void CSooRDPClientAppDlg::OnBnClickedCheckAutorun()
{
	m_bAutorunMstsc = IsDlgButtonChecked(IDC_CHECK_AUTORUN) == BST_CHECKED;
	if (m_bAutorunMstsc) {
		GetDlgItem(IDC_EDIT_MSTSC)->EnableWindow(FALSE);
		SetTimer(TM_AUTORUN, 3000, NULL);
		if (check_mstsc_running() && !m_bMstscConnected) {
			SetTimer(TM_CheckDeadMstsc, 15000, NULL);
		}
	}
	else {
		GetDlgItem(IDC_EDIT_MSTSC)->EnableWindow(TRUE);
		KillTimer(TM_AUTORUN);
	}
}


BOOL CSooRDPClientAppDlg::check_mstsc_running()
{
	if (m_hProcMstsc) {
		if (WAIT_TIMEOUT == WaitForSingleObject(m_hProcMstsc, 0)) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CSooRDPClientAppDlg::launch_mstsc()
{
	BOOL bOK = FALSE;
	CString strApp;
	GetDlgItemText(IDC_EDIT_MSTSC, strApp);

	if (m_hProcMstsc) {
		CloseHandle(m_hProcMstsc);
		m_hProcMstsc = NULL;
	}
	m_dwPidMstsc = 0;
	m_bMstscConnected = FALSE;
	KillTimer(TM_CheckDeadMstsc);
	if (m_bAutorunMstsc) {
		SetTimer(TM_CheckDeadMstsc, 15000, NULL);
	}
	bOK = launch_app(strApp, m_bUseMEDIUM_INTEGRITY, &m_hProcMstsc, &m_dwPidMstsc);
	if (!bOK) {
		KillTimer(TM_CheckDeadMstsc);
	}
	return bOK;
}

void CSooRDPClientAppDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TM_AUTORUN) {
		if (!m_bInstalled) {
			return;
		}
		if (check_mstsc_running()) {
			return;
		}
		m_strtmp.Format(LSTRW(RID_mstsc_pid_exited), m_dwPidMstsc);
		Log(m_strtmp);
		if (!launch_mstsc()) {
			KillTimer(nIDEvent);
			Log(LSTRW(RID_run_mstsc_err));
		}
		else {
			m_strtmp.Format(LSTRW(RID_mstsc_running), m_dwPidMstsc);
			Log(m_strtmp);
		}
	}
	else if (nIDEvent == TM_UpdataCfg) {
		UpdateConfig();
		KillTimer(nIDEvent);
	}
	else if (nIDEvent == TM_CheckDeadMstsc) {
		if (check_mstsc_running() && !m_bMstscConnected) {
			m_strtmp.Format(_T("因%d秒内未建立连接，强行终止mstsc进程%d"), const_MstscConnTimeout / 1000, m_dwPidMstsc);
			Log(m_strtmp);
			TerminateProcess(m_hProcMstsc, 300);
		}
		KillTimer(nIDEvent);
	}

	CDialogEx::OnTimer(nIDEvent);
}

void CSooRDPClientAppDlg::Log(LPCTSTR lpszText)
{
	CString strLine;
	CString t = CTime::GetCurrentTime().Format("%Y-%m-%d %H:%M:%S ");
	strLine.Format(_T("%s %s\r\n"), (LPCTSTR)t, lpszText);
	int nLength = m_editLog.GetWindowTextLength();
	m_editLog.SetSel(nLength, nLength);
	m_editLog.ReplaceSel(strLine);
}

afx_msg LRESULT CSooRDPClientAppDlg::OnMywmNotify(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case SooMsg_Connected:
	{
		m_bMstscConnected = TRUE;
		KillTimer(TM_CheckDeadMstsc);
	}
	break;
	case SooMsg_Disconnected:
	{
		m_bMstscConnected = FALSE;
		if (m_bAutorunMstsc) {
			if ((DWORD)lParam == m_dwPidMstsc) {
				SetTimer(TM_CheckDeadMstsc, 15000, NULL);
			}
			else {
				Log(_T("隧道异常，但是mstsc进程ID不匹配"));
			}
		}
	}
	break;
	case SooMsg_ChannelClosed: break;
	case SooMsg_Terminated: break;
	case SooMsg_OnNewChannelConnection: break;
	}
	return 0;
}

void CSooRDPClientAppDlg::OnEnUpdateEditN()
{
	CString str;
	m_editNotify.GetWindowText(str);
	Log(str);
}

void CSooRDPClientAppDlg::OnStnClickedLogo()
{
	CAboutDlg dlgAbout;
	dlgAbout.DoModal();
}

void CSooRDPClientAppDlg::OnEnUpdateConfig()
{ 
	SetTimer(TM_UpdataCfg, 2000, NULL);
}

void CSooRDPClientAppDlg::OnCbnSelchangeComboMode()
{ 
	switch (m_cbMode.GetCurSel()) {
	case 0:
		GetDlgItem(IDC_EDIT_RADDR)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_RPORT)->EnableWindow(TRUE);
		break;
	case 1:
		GetDlgItem(IDC_EDIT_RADDR)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_RPORT)->EnableWindow(FALSE);
		break;
	}
	SetTimer(TM_UpdataCfg, 2000, NULL);
}

void CSooRDPClientAppDlg::OnCancel()
{
}

void CSooRDPClientAppDlg::OnClose()
{
	CDialogEx::OnCancel();
}

void CSooRDPClientAppDlg::UpdateConfig()
{
	switch (m_cbMode.GetCurSel()) {
	case 0:
		m_pConfig->mode = rdccmode_conn;
		break;
	case 1:
		m_pConfig->mode = rdccmode_s5;
		break;
	}
	m_pConfig->port = (SHORT)GetDlgItemInt(IDC_EDIT_PORT);
	::GetDlgItemTextA(m_hWnd, IDC_EDIT_RADDR, m_pConfig->targetip, _countof(m_pConfig->targetip));
	m_pConfig->targetport = (int)GetDlgItemInt(IDC_EDIT_RPORT);

	if (check_mstsc_running()) {
		if (m_bMstscConnected)
			Log(LSTRW(RID_UpdateConfig_Connected));
		else
			Log(LSTRW(RID_UpdateConfig));
	}
}

static BOOL adjust_token_integrity_level(HANDLE token, const char* sid) {
	/* Convert the string SID to a SID *, then adjust the token's
	   privileges. */
	BOOL ret;
	PSID psd = NULL;
	if (ConvertStringSidToSidA(sid, &psd)) {
		TOKEN_MANDATORY_LABEL tml;

		ZeroMemory(&tml, sizeof(tml));
		tml.Label.Attributes = SE_GROUP_INTEGRITY;
		tml.Label.Sid = psd;

		ret = SetTokenInformation(token, TokenIntegrityLevel, &tml,
			sizeof(tml) + GetLengthSid(psd));

		LocalFree(psd);
	}
	return ret;
}

BOOL CSooRDPClientAppDlg::launch_app(LPCTSTR lpszCmd, BOOL bUseMediumIntegrity, HANDLE* pProcHandle, DWORD* pdwPid)
{
	BOOL bOK = FALSE;
	if (!bUseMediumIntegrity) {
		PROCESS_INFORMATION pi;
		STARTUPINFO si;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		if (CreateProcess(NULL, (LPWSTR)(LPCWSTR)lpszCmd, NULL,
			NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			bOK = TRUE;
			CloseHandle(pi.hThread);
			*pProcHandle = pi.hProcess;
			*pdwPid = pi.dwProcessId;
		}
		else {
			m_strError.Format(_T("CreateProcess err=%d"), GetLastError());
		}
		return bOK;
	}
	else {
		const char* requested_sid = "S-1-16-8192";
		HANDLE token_cur, token_dup;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE |
			TOKEN_ADJUST_DEFAULT |
			TOKEN_QUERY |
			TOKEN_ASSIGN_PRIMARY,
			&token_cur)) {
			if (DuplicateTokenEx(token_cur, 0, NULL, SecurityImpersonation,
				TokenPrimary, &token_dup)) {
				if (adjust_token_integrity_level(token_dup, requested_sid)) {
					PROCESS_INFORMATION pi;
					STARTUPINFO si;

					ZeroMemory(&si, sizeof(si));
					si.cb = sizeof(si);
					if (CreateProcessAsUser(token_dup, NULL, (LPWSTR)(LPCWSTR)lpszCmd, NULL,
						NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
						bOK = TRUE;
						CloseHandle(pi.hThread);
						*pProcHandle = pi.hProcess;
						*pdwPid = pi.dwProcessId;
					}
					else {
						m_strError.Format(_T("CreateProcessAsUser err=%d"), GetLastError());
					}
				}
				else {
					m_strError.Format(_T("adjust token integrity level err=%d"), GetLastError());
				}
				CloseHandle(token_dup);
			}
			else {
				m_strError.Format(_T("DuplicateTokenEx err=%d"), GetLastError());
			}
			CloseHandle(token_cur);
		}
		else {
			m_strError.Format(_T("OpenProcessToken err=%d"), GetLastError());
		}
		return bOK;
	}
}

BOOL FindPlugin(CString& path)
{
	WCHAR szCurrDir[MAX_PATH];

	DWORD dwRes = GetModuleFileNameW(NULL, szCurrDir, MAX_PATH);
	*wcsrchr(szCurrDir, '\\') = '\0';

	path = szCurrDir;
#ifdef _M_AMD64
	path += L"\\SooRDP-Plugin64.dll";
#elif defined _M_IX86
	path += L"\\SooRDP-Plugin32.dll"; 
#elif defined _M_ARM64 
	path += L"\\SooRDP-PluginARM64.dll";
#else
	return FALSE;xx
#endif
	return PathFileExists(path);
}

HRESULT RegisterPlugin(BOOL install)
{
	typedef HRESULT WINAPI DEF_reg();
	CString strDllPath;
	if (!FindPlugin(strDllPath)) {
		AfxMessageBox(_T("找不到SooRDP插件") + strDllPath);
		AfxThrowUserException();
	}

	HMODULE hDll = LoadLibrary(strDllPath);
	if (!hDll) {
		AfxMessageBox(_T("无法加载SooRDP插件"));
		AfxThrowUserException();
	}

	DEF_reg* reg = NULL;
	if (install) {
		*(void**)&reg = GetProcAddress(hDll, "DllRegisterServer");
	}
	else {
		*(void**)&reg = GetProcAddress(hDll, "DllUnregisterServer");
	}

	HRESULT hr = reg();
	if (install) {
		if (FAILED(hr)) {
			int ret = AfxMessageBox(_T("无法注册SooRDP插件，可能是权限不足。是否继续？（如果你已经手工regsvr32注册了SooRDP-Plugin则可以继续。）"), MB_YESNO | MB_ICONWARNING);
			if (IDYES != ret) {
				AfxThrowUserException();
			}
		}
	}
	FreeLibrary(hDll);
	return hr;
}

void CSooRDPClientAppDlg::OnBnClickedButtonChooseRdp()
{
	CFileDialog dlg(TRUE, _T("*.rdp"), NULL, OFN_EXPLORER);
	dlg.m_ofn.lpstrFilter = LSTRW(RID_ChooseRDPFilter);
	dlg.m_ofn.lpstrTitle = LSTRW(RID_ChooseRDPTitle);

	if (dlg.DoModal() == IDOK)
	{
		CString cmd;
		cmd.Format(_T("mstsc \"%s\""), (LPCTSTR)dlg.GetPathName());
		SetDlgItemText(IDC_EDIT_MSTSC, cmd);
	}
}
