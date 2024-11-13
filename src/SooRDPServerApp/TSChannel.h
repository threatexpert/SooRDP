#pragma once

#include <wtsapi32.h>
#include <pchannel.h>
#include "../misc/Pipe.h"
#include "../misc/NBSocket.h"

BOOL supportDynamicVirtualChannel();

class CTSChannelSocketWrapper
{
	void* m_priv;
	UINT64 m_totalWritten, m_totalRead;
public:
	Cnbsocket* m_psocket;

	CTSChannelSocketWrapper();
	~CTSChannelSocketWrapper();

	BOOL Open(LPCSTR lpszName, BOOL bDVC);
	void Close();
	HANDLE GetExitEventHandle();
	void SetFlagExit(BOOL* p);

	bool WriteAll(const void* data, int len, int timeout_sec = -1);
	int Read(void* buf, int size, int timeout_sec);
	void GetStat(UINT64* W, UINT64* R);
};