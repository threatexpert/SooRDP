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
			L"请使用64位版本。",
			L"Please use the 64-bit version."
		);
		casexx(
			RID_TITLE,
			L"SooRDP隧道客户端",
			L"SooRDP Tunnel Client"
		);
		casexx(
			RID_MODE_FORWARD,
			L"远程目的地址",
			L"destination address"
		);
		casexx(
			RID_MODE_SOCKS5,
			L"Socks5代理",
			L"Socks5 Proxy"
		);
		casexx(
			RID_ChooseRDPFilter,
			L"远程桌面文件 (*.RDP)\0*.rdp\0\0",
			L"Remote Desktop Protocol File (*.RDP)\0*.rdp\0\0"
		);
		casexx(
			RID_ChooseRDPTitle,
			L"选择文件后缀为.rdp的远程桌面文件",
			L"Select the file with the file extension .rdp"
		);
		casexx(
			RID_UpdateConfig_Connected,
			L"配置已更新，如果已建立隧道需要服务端程序按停止按钮再开启按钮才生效。",
			L"The configuration has been updated. If a tunnel has been established, the server program needs to press the stop button and then the start button to take effect."
		);
		casexx(
			RID_UpdateConfig,
			L"配置已更新",
			L"The configuration has been updated."
		);
		casexx(
			RID_run_mstsc_err,
			L"启动mstsc失败。",
			L"Failed to start mstsc."
		);
		casexx(
			RID_mstsc_running,
			L"创建了mstsc进程%d...",
			L"Created mstsc process %d..."
		);
		casexx(
			RID_mstsc_pid_exited,
			L"进程%d已退出.",
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
