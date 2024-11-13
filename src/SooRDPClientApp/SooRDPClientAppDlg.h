
// SooRDPClientAppDlg.h: 头文件
//

#pragma once

#include "../SooRDP-Plugin/idata.h"
#include "../misc/memshare.h"

// CSooRDPClientAppDlg 对话框
class CSooRDPClientAppDlg : public CDialogEx
{
// 构造
public:
	CSooRDPClientAppDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SOORDPCLIENTAPP_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;
	Cmemshare m_shm_InsCount;
	DWORD* m_pInsCount;
	Cmemshare m_shm;
	_SharedMem* m_pConfig;
	BOOL m_bInstalled;
	BOOL m_bUseMEDIUM_INTEGRITY;
	HANDLE m_hProcMstsc;
	DWORD m_dwPidMstsc;
	BOOL m_bMstscConnected;
	CString m_strError;
	CString m_strtmp;
	BOOL m_bAutorunMstsc;
	CEdit m_editLog;
	CEdit m_editNotify;
	CComboBox m_cbMode;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedCheckAutorun();

	BOOL check_mstsc_running();
	BOOL launch_mstsc();
	BOOL launch_app(LPCTSTR lpszCmd, BOOL bUseMediumIntegrity, HANDLE *pProcHandle, DWORD *pdwPid);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnMywmNotify(WPARAM wParam, LPARAM lParam);

	void Log(LPCTSTR lpszText);
	afx_msg void OnEnUpdateEditN();
	afx_msg void OnStnClickedLogo();
	afx_msg void OnEnUpdateConfig();
	afx_msg void OnBnClickedOpen();
	virtual void OnCancel();
	afx_msg void OnClose();
	afx_msg void OnCbnSelchangeComboMode();
	void UpdateConfig();
	afx_msg void OnBnClickedButtonChooseRdp();
};

BOOL FindPlugin(CString& path);
HRESULT RegisterPlugin(BOOL install);
