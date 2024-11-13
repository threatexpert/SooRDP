#include "pch.h"
#include <assert.h>
#include "langstr.h"

const int default_lang = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

#define strxx(dl, en) {\
    if (lid == default_lang) { return dl; }\
    else { return en; }\
}

#define casexx(i, dl, en) \
case i:\
strxx(\
dl, \
en)

LPCWSTR LSTRW_X(int i, int lid)
{
	switch (i) {
		casexx(
			RID_TunnelNotReady,
			L"�����ʱδ׼�������������ر�.",
			L"The tunnel timed out and was not ready, so it was closed actively."
		);
		casexx(
			RID_TunnelReady,
			L"���׼������.",
			L"The tunnel is ready."
		);
		casexx(
			RID_TunnelKeepaliveTimeout,
			L"���Keepalive��ʱ�������ر�.",
			L"The tunnel Keepalive timed out, close it."
		);
		casexx(
			RID_TunnelDataChannelClosed,
			L"��������ѹر�.",
			L"Data channel is closed."
		);
		casexx(
			RID_SVCSocketClosed,
			L"SVC���Socket�ر�.",
			L"The SVC tunnel socket is closed."
		);
		casexx(
			RID_SVCTimeoutClose,
			L"SVC���Keepalive��ʱ�������ر�.",
			L"The SVC tunnel Keepalive times out, close it."
		);
		casexx(
			RID_LocallyOpenSVCTunnel,
			L"���ش�SVC���...",
			L"Open the SVC tunnel locally..."
		);
		casexx(
			RID_LocallyOpenSVCTunnelFailed,
			L"���ش�SVC���ʧ��(%d).",
			L"Failed to open the SVC tunnel locally (%d)."
		);
		casexx(
			RID_AskingMakeSVCTunnel,
			L"Զ����������������SVC���.",
			L"The remote desktop is requesting to establish an SVC tunnel."
		);
		casexx(
			RID_AskingMakeDVCTunnel,
			L"Զ����������������DVC���.",
			L"The remote desktop is requesting to establish an DVC tunnel."
		);
		casexx(
			RID_TunnelErr_closing,
			L"����쳣����ֹ...",
			L"Tunnel error, closing..."
		);
		casexx(
			RID_Tunnel_Init_Done,
			L"��ʼ��SVC�����ɣ�ӳ�䵽���ض˿�%d.",
			L"Initialization of SVC tunnel completed, mapped to local port %d."
		);
		casexx(
			RID_Tunnel_Init_Err,
			L"��ʼ��SoTunnel�쳣: %s.",
			L"Exception in initializing SoTunnel: %s."
		);
		casexx(
			RID_Process_PID,
			L"[����%d] %s",
			L"[Process%d] %s"
		);
		casexx(
			RID_InitPlugFaided,
			L"��ʼ��SooRDP���ʧ��.",
			L"Failed to initialize SooRDP plugin."
		);
		casexx(
			RID_InitPlugDone,
			L"��ʼ��SooRDP����ɹ�.",
			L"Initialized SooRDP plugin successfully."
		);
		casexx(
			RID_RDPConnected,
			L"Զ������������.",
			L"Remote Desktop Connected."
		);
		casexx(
			RID_RDPDisconnected,
			L"Զ�������ѶϿ�(%d).",
			L"Remote Desktop disconnected (%d)."
		);
		casexx(
			RID_RDPTerminated,
			L"Զ����������ֹ.",
			L"Remote Desktop terminated."
		);
		casexx(
			RID_ClosePrevTunnel,
			L"�ر���һ�����...",
			L"Closing the previous tunnel..."
		);
		casexx(
			RID_AskingMakeChannelFailed,
			L"Զ������������������������س�ʼ���쳣.",
			L"The remote desktop requests to establish a data tunnel, but the local initialization is failed."
		);
		casexx(
			RID_InitDVCTunnelDone,
			L"��ʼ��DVC�����ɣ�ӳ�䵽���ض˿�%d.",
			L"Initialization of DVC tunnel completed, mapped to local port %d."
		);

	default:
		assert(false);
	}
	return L"";
}

LPCWSTR LSTRW(int i)
{
	LANGID lid = GetUserDefaultUILanguage();
	return LSTRW_X(i, lid);
}
