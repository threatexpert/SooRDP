#pragma once

#define SharedMemConfig "SooRDPShmCfg"

#pragma pack(push, 1)
struct _SharedMem
{
	unsigned short port;
	unsigned char mode;
	char targetip[64];
	unsigned short targetport;
	HWND hWnd;
	UINT uMsg;
	HWND hText;
};
#define rdccmode_conn 1
#define rdccmode_s5   2

#pragma pack(pop)

enum SooMsg {
	SooMsg_base = 0,
	SooMsg_Connected,
	SooMsg_Disconnected,
	SooMsg_ChannelClosed,
	SooMsg_Terminated,
	SooMsg_OnNewChannelConnection,
};

extern void sendLog(LPCWSTR pctszFormat, ...);
