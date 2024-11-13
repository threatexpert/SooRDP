// TsClientX.cpp: CTsClientX 的实现

#include "pch.h"
#include "TsClientX.h"
#include "SooRDP-SVC.h"

#pragma comment(lib, "Ws2_32.lib")

Cmemshare g_shm;
_SharedMem* gshmconfig = NULL;

void sendLog(LPCWSTR pctszFormat, ...)
{
	if (!gshmconfig)
		return;
	CString s;
	va_list argList;
	va_start(argList, pctszFormat);
	s.FormatV(pctszFormat, argList);
	va_end(argList);
	CString s2;
	s2.Format(LSTRW(RID_Process_PID), GetCurrentProcessId(), (LPCWSTR)s);
	::SendMessageW(gshmconfig->hText, WM_SETTEXT, 0, (LPARAM)(LPCWSTR)s2);
}


// CTsClientX

HRESULT CTsClientX::FinalConstruct()
{
	char var[128] = "";
	GetEnvironmentVariableA(SharedMemConfig, var, _countof(var));
	if (!var[0]) {
		strcpy_s(var, SharedMemConfig);
	}

	ATLTRACE("CTsClientX::FinalConstruct shm=%s\n", var);

	if (g_shm.GetMem() == NULL) {
		if (!g_shm.Open(var, sizeof(_SharedMem))) {
			ATLTRACE("CTsClientX::FinalConstruct g_shm.Open failed\n");
			return E_FAIL;
		}
	}
	gshmconfig = m_pConfig = (_SharedMem*)g_shm.GetMem();
	return S_OK;
}

void CTsClientX::FinalRelease()
{

}

HRESULT STDMETHODCALLTYPE CTsClientX::Initialize(IWTSVirtualChannelManager* pChannelMgr)
{
	HRESULT	hr;
	CComPtr<IWTSListener> ptrListener;
	CComObject<CSampleListenerCallback>* pListenerCallback;

	if (!m_pConfig) {
		ATLTRACE("CTsClientX::Initialize m_pConfig is NULL\n");
		return E_FAIL;
	}

	CComObject<CSampleListenerCallback>::CreateInstance(&pListenerCallback);
	m_ptrListenerCallback_Data = pListenerCallback;
	m_ptrListenerCallback_Data->m_pNotify = this;
	m_ptrListenerCallback_Data->m_userdata = (CSampleListenerCallback*)m_ptrListenerCallback_Data;
	pListenerCallback = NULL;

	CString strLog;

	hr = pChannelMgr->CreateListener(
		"SOORDPD",
		0,
		m_ptrListenerCallback_Data,
		&ptrListener);
	if (FAILED(hr)) {
		ATLTRACE("CTsClientX::Initialize CreateListener failed\n");
		sendLog(LSTRW(RID_InitPlugFaided));
		return hr;
	}

	ATLTRACE("CTsClientX::Initialize GOOD\n");
	sendLog(LSTRW(RID_InitPlugDone));
	return hr;
}

HRESULT STDMETHODCALLTYPE CTsClientX::Connected()
{
	sendLog(LSTRW(RID_RDPConnected));
	::PostMessage(gshmconfig->hWnd, gshmconfig->uMsg, SooMsg_Connected, (LPARAM)GetCurrentProcessId());
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CTsClientX::Disconnected(DWORD dwDisconnectCode)
{
	sendLog(LSTRW(RID_RDPDisconnected), dwDisconnectCode);
	::PostMessage(gshmconfig->hWnd, gshmconfig->uMsg, SooMsg_Disconnected, (LPARAM)GetCurrentProcessId());
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CTsClientX::Terminated()
{
	sendLog(LSTRW(RID_RDPTerminated));
	::PostMessage(gshmconfig->hWnd, gshmconfig->uMsg, SooMsg_Terminated, (LPARAM)GetCurrentProcessId());
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CTsClientX::OnNewChannelConnection(IWTSVirtualChannel* pChannel, BSTR data, BOOL* pbAccept, IWTSVirtualChannelCallback** ppCallback)
{
	HRESULT hr = S_OK;

	if ((CSampleListenerCallback*)data == (CSampleListenerCallback*)m_ptrListenerCallback_Data) {
		ATLTRACE("CTsClientX::OnNewChannelConnection ctrl\n");
		sendLog(LSTRW(RID_AskingMakeDVCTunnel));
		::PostMessage(gshmconfig->hWnd, gshmconfig->uMsg, SooMsg_OnNewChannelConnection, (LPARAM)GetCurrentProcessId());

		CloseTunnel_SVC();
		if (m_ptrRDC) {
			sendLog(LSTRW(RID_ClosePrevTunnel));
			m_ptrRDC->CloseChannel();
			m_ptrRDC = NULL;
		}

		CComObject<CSooRDC>* pCallback;

		*pbAccept = FALSE;

		if (!m_pConfig) {
			return S_OK;
		}

		hr = CComObject<CSooRDC>::CreateInstance(&pCallback);
		if (FAILED(hr)) {
			return hr;
		}

		m_ptrRDC = pCallback;
		if (!m_ptrRDC->SetChannel(pChannel)) {
			sendLog(LSTRW(RID_AskingMakeChannelFailed));
			return S_OK;
		}

		*ppCallback = m_ptrRDC;
		(*ppCallback)->AddRef();

		*pbAccept = TRUE;
		sendLog(LSTRW(RID_InitDVCTunnelDone), m_pConfig->port);
	}

	return hr;
}
