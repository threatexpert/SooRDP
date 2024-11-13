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
			L"隧道超时未准备就绪，主动关闭.",
			L"The tunnel timed out and was not ready, so it was closed actively."
		);
		casexx(
			RID_TunnelReady,
			L"隧道准备就绪.",
			L"The tunnel is ready."
		);
		casexx(
			RID_TunnelKeepaliveTimeout,
			L"隧道Keepalive超时，主动关闭.",
			L"The tunnel Keepalive timed out, close it."
		);
		casexx(
			RID_TunnelDataChannelClosed,
			L"数据隧道已关闭.",
			L"Data channel is closed."
		);
		casexx(
			RID_SVCSocketClosed,
			L"SVC隧道Socket关闭.",
			L"The SVC tunnel socket is closed."
		);
		casexx(
			RID_SVCTimeoutClose,
			L"SVC隧道Keepalive超时，主动关闭.",
			L"The SVC tunnel Keepalive times out, close it."
		);
		casexx(
			RID_LocallyOpenSVCTunnel,
			L"本地打开SVC隧道...",
			L"Open the SVC tunnel locally..."
		);
		casexx(
			RID_LocallyOpenSVCTunnelFailed,
			L"本地打开SVC隧道失败(%d).",
			L"Failed to open the SVC tunnel locally (%d)."
		);
		casexx(
			RID_AskingMakeSVCTunnel,
			L"远程桌面正在请求建立SVC隧道.",
			L"The remote desktop is requesting to establish an SVC tunnel."
		);
		casexx(
			RID_AskingMakeDVCTunnel,
			L"远程桌面正在请求建立DVC隧道.",
			L"The remote desktop is requesting to establish an DVC tunnel."
		);
		casexx(
			RID_TunnelErr_closing,
			L"隧道异常，终止...",
			L"Tunnel error, closing..."
		);
		casexx(
			RID_Tunnel_Init_Done,
			L"初始化SVC隧道完成，映射到本地端口%d.",
			L"Initialization of SVC tunnel completed, mapped to local port %d."
		);
		casexx(
			RID_Tunnel_Init_Err,
			L"初始化SoTunnel异常: %s.",
			L"Exception in initializing SoTunnel: %s."
		);
		casexx(
			RID_Process_PID,
			L"[进程%d] %s",
			L"[Process%d] %s"
		);
		casexx(
			RID_InitPlugFaided,
			L"初始化SooRDP插件失败.",
			L"Failed to initialize SooRDP plugin."
		);
		casexx(
			RID_InitPlugDone,
			L"初始化SooRDP插件成功.",
			L"Initialized SooRDP plugin successfully."
		);
		casexx(
			RID_RDPConnected,
			L"远程桌面已连接.",
			L"Remote Desktop Connected."
		);
		casexx(
			RID_RDPDisconnected,
			L"远程桌面已断开(%d).",
			L"Remote Desktop disconnected (%d)."
		);
		casexx(
			RID_RDPTerminated,
			L"远程桌面已终止.",
			L"Remote Desktop terminated."
		);
		casexx(
			RID_ClosePrevTunnel,
			L"关闭上一个隧道...",
			L"Closing the previous tunnel..."
		);
		casexx(
			RID_AskingMakeChannelFailed,
			L"远程桌面请求建立数据隧道，本地初始化异常.",
			L"The remote desktop requests to establish a data tunnel, but the local initialization is failed."
		);
		casexx(
			RID_InitDVCTunnelDone,
			L"初始化DVC隧道完成，映射到本地端口%d.",
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
