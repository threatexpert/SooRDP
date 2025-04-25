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
			RID_Start,
			L"启动",
			L"Start"
		);
		casexx(
			RID_Stop,
			L"停止",
			L"Stop"
		);
		casexx(
			RID_Stopping,
			L"停止中...",
			L"Stopping"
		);
		casexx(
			RID_NotReady,
			L"隧道未准备就绪，重置...",
			L"Tunnel not ready, resetting..."
		);
		casexx(
			RID_TunnelSocketClosed,
			L"隧道Socket关闭.",
			L"Tunnel socket closed."
		);
		casexx(
			RID_TunnelModeChanged_mode_s5,
			L"RDP隧道#%d模式为：提供Socks5服务",
			L"RDP tunnel#%d mode is: providing Socks5 service"
		);
		casexx(
			RID_TunnelModeChanged_mode_fwd,
			L"RDP隧道#%d模式为：转发至目的%S:%d",
			L"RDP tunnel#%d mode is: forwarding to %S:%d"
		);
		casexx(
			RID_Tunnel_S5_connect_to,
			L"Socks5: 连接%S:%d...",
			L"Socks5: Connecting to %S:%d..."
		);
		casexx(
			RID_Tunnel_Ping_timeout,
			L"RDP隧道#%d Keepalive超时，主动关闭",
			L"RDP tunnel #%d Keepalive timed out, closing"
		);
		casexx(
			RID_Creating_DVC,
			L"创建(DVC)RDP隧道#%d...",
			L"Creating (DVC) RDP tunnel #%d..."
		);
		casexx(
			RID_Creating_SVC,
			L"创建(SVC)RDP隧道#%d...",
			L"Creating (SVC) RDP tunnel #%d..."
		);
		casexx(
			RID_Creating_Tunnel_Failed,
			L"创建RDP隧道#%d失败, 错误%d",
			L"Failed to create RDP tunnel #%d, error %d"
		);
		casexx(
			RID_Creating_FWD_Failed,
			L"创建转发端口的RDP隧道#%d失败, %s",
			L"Failed to create RDP tunnel #%d for tcp forwarding, %s"
		);
		casexx(
			RID_Tunnel_End,
			L"RDP隧道#%d结束...",
			L"RDP tunnel #%d ended..."
		);
		casexx(
			RID_Title,
			L"SooRDP隧道服务端",
			L"SooRDP Tunnel Server"
		);
		casexx(
			RID_Tunnel_Status,
			L"RDP隧道#%d已转发数据: 下行%S,上行%S,TCP连接数%d",
			L"RDP tunnel#%d statistics: down %S, up %S, TCP:%d"
		);
		casexx(
			RID_Tunnel_Dynamic,
			L"动态",
			L"Dynamic"
		);
		casexx(
			RID_Tunnel_Static,
			L"静态",
			L"Static"
		);
		casexx(
			RID_Tunnel_PRI_MED,
			L"一般",
			L"MED"
		);
		casexx(
			RID_Tunnel_PRI_HIGH,
			L"紧急",
			L"HIGH"
		);
		casexx(
			RID_Tunnel_PRI_REAL,
			L"实时",
			L"REAL"
		);
		casexx(
			RID_Tunnel_COMPRESS,
			L"压缩",
			L"COMPRESS"
		);
		casexx(
			RID_Tunnel_NOCOMPRESS,
			L"不压缩",
			L"NOCOMPRESS"
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
