#pragma once


BOOL
APIENTRY
MyCreatePipeEx(
	OUT LPHANDLE lpReadPipe,
	OUT LPHANDLE lpWritePipe,
	IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
	IN DWORD nSize,
	DWORD dwReadMode,
	DWORD dwWriteMode
);

typedef
BOOL
WINAPI
DEF_CancelIoEx(
	HANDLE hFile,
	LPOVERLAPPED lpOverlapped
);

class CPipe
{
protected:
	HANDLE m_h;
	OVERLAPPED  OverlappedWrite, OverlappedRead;
	UINT64 m_totalWritten, m_totalRead;
	BOOL m_bCompatibleWithOlderOS;
	static DEF_CancelIoEx* _CancelIoEx;
public:

	enum {
		TIMEOUT = -11
	};
	CPipe();
	~CPipe();
	void Attach(HANDLE h);
	HANDLE Detach();

	BOOL CompatibleWithOlderOS(BOOL b);
	int Read(void *buf, int bufsize, DWORD timeout_ms= INFINITE);
	int Write(const void* data, int len, DWORD timeout_ms= INFINITE);
	void Close();
	void GetStat(UINT64* W, UINT64* R);
	virtual void ClosePipeHanle(HANDLE h);

};
