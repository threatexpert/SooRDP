#include "pch.h"
#include "SooRDChannel.h"
#include "idata.h"
#include "TsClientX.h"

extern _SharedMem* gshmconfig;


void CSoTunnelRDC::OnChannelSocketReady(bool ok)
{
	if (!ok) {
		sendLog(LSTRW(RID_TunnelNotReady));
		m_ptrChannel->Close();
	}
	else {
		sendLog(LSTRW(RID_TunnelReady));
		SendConfig(gshmconfig->mode, gshmconfig->targetip, gshmconfig->targetport);
	}
}

void CSoTunnelRDC::OnChannelSocketClose()
{
	m_ptrChannel->Close();
}

void CSoTunnelRDC::OnChannelPingTimeout()
{
	sendLog(LSTRW(RID_TunnelKeepaliveTimeout));
	m_ptrChannel->Close();
}

HRESULT CSooRDC::FinalConstruct()
{
	m_bChannelCloseFlag = FALSE;
	m_totalReceived = 0;
	m_pSoTunnel = NULL;
	m_pS2tSocket = NULL;
	return S_OK;
}

void CSooRDC::FinalRelease()
{
	CloseChannel();
}

BOOL CSooRDC::SetChannel(IWTSVirtualChannel* pChannel)
{
	ATLTRACE("CSooRDC::SetChannel....\n");

	m_pSoTunnel = new CSoTunnelRDC();
	m_pSoTunnel->EnableKeepalive(20);
	if (!m_pSoTunnel->InitLocal(gshmconfig->port)) {
		ATLTRACE("CSooRDC::SetChannel, InitLocal failed\n");
		sendLog(_T("³õÊ¼»¯SoTunnelÒì³£: %s."), m_pSoTunnel->m_lasterr.c_str());
		return FALSE;
	}
	m_pSoTunnel->m_ptrChannel = pChannel;
	m_ptrChannel = pChannel;

	m_pS2tSocket = new CSotunnel2TsChannel();
	m_pS2tSocket->m_hSocket = m_pSoTunnel->m_pTunnelIO->Detach();
	m_pS2tSocket->m_ptrChannel = m_ptrChannel;
	if (!m_pSoTunnel->AddASocket(m_pS2tSocket, FD_READ | FD_CLOSE)) {
		m_ptrChannel->Close();
		return FALSE;
	}

	return TRUE;
}

HRESULT STDMETHODCALLTYPE CSooRDC::OnDataReceived(ULONG cbSize, BYTE* pBuffer)
{
	//ATLTRACE("CSooRDC::OnDataReceived(%d)\n", cbSize);
	if (m_bChannelCloseFlag) {
		ATLTRACE("CSooRDC::OnDataReceived err 1\n");
		m_ptrChannel->Close();
		return E_FAIL;
	}
	if (m_totalReceived == 0) {
		ATLTRACE("CSooRDC::OnDataReceived(%d) first block\n", cbSize);
	}

	m_totalReceived += cbSize;
	if (!socket_sendall(m_pS2tSocket->m_hSocket, pBuffer, cbSize, -1)) {
		ATLTRACE("CSooRDC::OnDataReceived err 3\n");
		m_ptrChannel->Close();
		return E_FAIL;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CSooRDC::OnClose()
{
	ATLTRACE("CSooRDC::OnClose, totalrecvd=%I64d\n", m_totalReceived);
	sendLog(LSTRW(RID_TunnelDataChannelClosed));
	::PostMessage(gshmconfig->hWnd, gshmconfig->uMsg, SooMsg_ChannelClosed, (LPARAM)GetCurrentProcessId());
	CloseChannel();
	return S_OK;
}

void CSooRDC::CloseChannel()
{
	if (m_pSoTunnel) {
		delete m_pSoTunnel;
		m_pSoTunnel = NULL;
	}

	if (m_pS2tSocket) {
		delete m_pS2tSocket;
		m_pS2tSocket = NULL;
	}
	if (m_ptrChannel) {
		m_ptrChannel->Close();
		m_ptrChannel = NULL;
	}
}

CSotunnel2TsChannel::CSotunnel2TsChannel()
{
	m_readbuf = xbuf_create(bufsize);
}

CSotunnel2TsChannel::~CSotunnel2TsChannel()
{
	xbuf_free(m_readbuf);
}

void CSotunnel2TsChannel::OnRead(int err)
{
	if (err != 0) {
		CloseChannel();
		return;
	}
	int bufavail = xbuf_avail(m_readbuf);
	int ret = recv(m_hSocket, xbuf_datatail(m_readbuf), bufavail, 0);
	if (ret < 0) {
		if (WSAEWOULDBLOCK == WSAGetLastError()) {
			return;
		}
		CloseChannel();
		return;
	}
	else if (ret == 0) {
		CloseChannel();
		return;
	}

	xbuf_appended(m_readbuf, ret);
	if (FAILED(m_ptrChannel->Write(m_readbuf->datalen, (BYTE*)xbuf_data(m_readbuf), NULL))) {
		CloseChannel();
		return;
	}
	xbuf_pos_forward(m_readbuf, m_readbuf->datalen);
}


void CSotunnel2TsChannel::OnClose(int err)
{
	if (err != 0) {
		CloseChannel();
		return;
	}
	char buf[1024];
	while (1) {
		int ret = recv(m_hSocket, buf, sizeof(buf), 0);
		if (ret <= 0) {
			break;
		}
		if (FAILED(m_ptrChannel->Write(ret, (BYTE*)buf, NULL))) {
			break;
		}
	}
	CloseChannel();
}

void CSotunnel2TsChannel::OnRelease()
{
}

void CSotunnel2TsChannel::CloseChannel()
{
	Close();
	m_ptrChannel->Close();
}
