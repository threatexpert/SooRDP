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
			L"����",
			L"Start"
		);
		casexx(
			RID_Stop,
			L"ֹͣ",
			L"Stop"
		);
		casexx(
			RID_Stopping,
			L"ֹͣ��...",
			L"Stopping"
		);
		casexx(
			RID_NotReady,
			L"���δ׼������������...",
			L"Tunnel not ready, resetting..."
		);
		casexx(
			RID_TunnelSocketClosed,
			L"���Socket�ر�.",
			L"Tunnel socket closed."
		);
		casexx(
			RID_TunnelModeChanged_mode_s5,
			L"RDP���#%dģʽΪ���ṩSocks5����",
			L"RDP tunnel#%d mode is: providing Socks5 service"
		);
		casexx(
			RID_TunnelModeChanged_mode_fwd,
			L"RDP���#%dģʽΪ��ת����Ŀ��%S:%d",
			L"RDP tunnel#%d mode is: forwarding to %S:%d"
		);
		casexx(
			RID_Tunnel_S5_connect_to,
			L"Socks5: ����%S:%d...",
			L"Socks5: Connecting to %S:%d..."
		);
		casexx(
			RID_Tunnel_Ping_timeout,
			L"RDP���#%d Keepalive��ʱ�������ر�",
			L"RDP tunnel #%d Keepalive timed out, closing"
		);
		casexx(
			RID_Creating_DVC,
			L"����(DVC)RDP���#%d...",
			L"Creating (DVC) RDP tunnel #%d..."
		);
		casexx(
			RID_Creating_SVC,
			L"����(SVC)RDP���#%d...",
			L"Creating (SVC) RDP tunnel #%d..."
		);
		casexx(
			RID_Creating_Tunnel_Failed,
			L"����RDP���#%dʧ��, ����%d",
			L"Failed to create RDP tunnel #%d, error %d"
		);
		casexx(
			RID_Creating_FWD_Failed,
			L"����ת���˿ڵ�RDP���#%dʧ��, %s",
			L"Failed to create RDP tunnel #%d for tcp forwarding, %s"
		);
		casexx(
			RID_Tunnel_End,
			L"RDP���#%d����...",
			L"RDP tunnel #%d ended..."
		);
		casexx(
			RID_Title,
			L"SooRDP��������",
			L"SooRDP Tunnel Server"
		);
		casexx(
			RID_Tunnel_Status,
			L"RDP���#%d��ת������: ����%S,����%S,TCP������%d",
			L"RDP tunnel#%d statistics: down %S, up %S, TCP:%d"
		);
		casexx(
			RID_Tunnel_Dynamic,
			L"��̬",
			L"Dynamic"
		);
		casexx(
			RID_Tunnel_Static,
			L"��̬",
			L"Static"
		);
		casexx(
			RID_Tunnel_PRI_MED,
			L"һ��",
			L"MED"
		);
		casexx(
			RID_Tunnel_PRI_HIGH,
			L"����",
			L"HIGH"
		);
		casexx(
			RID_Tunnel_PRI_REAL,
			L"ʵʱ",
			L"REAL"
		);
		casexx(
			RID_Tunnel_COMPRESS,
			L"ѹ��",
			L"COMPRESS"
		);
		casexx(
			RID_Tunnel_NOCOMPRESS,
			L"��ѹ��",
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
