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
			RID_UseX64Version,
			L"��ʹ��64λ�汾��",
			L"Please use the 64-bit version."
		);
		casexx(
			RID_TITLE,
			L"SooRDP����ͻ���",
			L"SooRDP Tunnel Client"
		);
		casexx(
			RID_MODE_FORWARD,
			L"Զ��Ŀ�ĵ�ַ",
			L"destination address"
		);
		casexx(
			RID_MODE_SOCKS5,
			L"Socks5����",
			L"Socks5 Proxy"
		);
		casexx(
			RID_ChooseRDPFilter,
			L"Զ�������ļ� (*.RDP)\0*.rdp\0\0",
			L"Remote Desktop Protocol File (*.RDP)\0*.rdp\0\0"
		);
		casexx(
			RID_ChooseRDPTitle,
			L"ѡ���ļ���׺Ϊ.rdp��Զ�������ļ�",
			L"Select the file with the file extension .rdp"
		);
		casexx(
			RID_UpdateConfig_Connected,
			L"�����Ѹ��£�����ѽ��������Ҫ����˳���ֹͣ��ť�ٿ�����ť����Ч��",
			L"The configuration has been updated. If a tunnel has been established, the server program needs to press the stop button and then the start button to take effect."
		);
		casexx(
			RID_UpdateConfig,
			L"�����Ѹ���",
			L"The configuration has been updated."
		);
		casexx(
			RID_run_mstsc_err,
			L"����mstscʧ�ܡ�",
			L"Failed to start mstsc."
		);
		casexx(
			RID_mstsc_running,
			L"������mstsc����%d...",
			L"Created mstsc process %d..."
		);
		casexx(
			RID_mstsc_pid_exited,
			L"����%d���˳�.",
			L"Process %d has exited."
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
