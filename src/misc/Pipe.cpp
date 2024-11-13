#include "pch.h"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "Pipe.h"

static ULONG PipeSerialNumber = 0;


BOOL
APIENTRY
MyCreatePipeEx(
	OUT LPHANDLE lpReadPipe,
	OUT LPHANDLE lpWritePipe,
	IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
	IN DWORD nSize,
	DWORD dwReadMode,
	DWORD dwWriteMode
)

/*++
Routine Description:
	The CreatePipeEx API is used to create an anonymous pipe I/O device.
	Unlike CreatePipe FILE_FLAG_OVERLAPPED may be specified for one or
	both handles.
	Two handles to the device are created.  One handle is opened for
	reading and the other is opened for writing.  These handles may be
	used in subsequent calls to ReadFile and WriteFile to transmit data
	through the pipe.
Arguments:
	lpReadPipe - Returns a handle to the read side of the pipe.  Data
		may be read from the pipe by specifying this handle value in a
		subsequent call to ReadFile.
	lpWritePipe - Returns a handle to the write side of the pipe.  Data
		may be written to the pipe by specifying this handle value in a
		subsequent call to WriteFile.
	lpPipeAttributes - An optional parameter that may be used to specify
		the attributes of the new pipe.  If the parameter is not
		specified, then the pipe is created without a security
		descriptor, and the resulting handles are not inherited on
		process creation.  Otherwise, the optional security attributes
		are used on the pipe, and the inherit handles flag effects both
		pipe handles.
	nSize - Supplies the requested buffer size for the pipe.  This is
		only a suggestion and is used by the operating system to
		calculate an appropriate buffering mechanism.  A value of zero
		indicates that the system is to choose the default buffering
		scheme.
Return Value:
	TRUE - The operation was successful.
	FALSE/NULL - The operation failed. Extended error status is available
		using GetLastError.
--*/

{
	HANDLE ReadPipeHandle, WritePipeHandle;
	DWORD dwError;
	CHAR PipeNameBuffer[MAX_PATH];

	//
	// Only one valid OpenMode flag - FILE_FLAG_OVERLAPPED
	//

	if ((dwReadMode | dwWriteMode) & (~FILE_FLAG_OVERLAPPED)) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	//
	//  Set the default timeout to 120 seconds
	//

	if (nSize == 0) {
		nSize = 4096;
	}

	sprintf_s(PipeNameBuffer, MAX_PATH,
		"\\\\.\\Pipe\\RemoteExeAnon.%08x.%08x",
		GetCurrentProcessId(),
		InterlockedIncrement(&PipeSerialNumber)
	);

	ReadPipeHandle = CreateNamedPipeA(
		PipeNameBuffer,
		PIPE_ACCESS_INBOUND | dwReadMode,
		PIPE_TYPE_BYTE | PIPE_WAIT,
		1,             // Number of pipes
		nSize,         // Out buffer size
		nSize,         // In buffer size
		120 * 1000,    // Timeout in ms
		lpPipeAttributes
	);

	if (!ReadPipeHandle) {
		return FALSE;
	}

	WritePipeHandle = CreateFileA(
		PipeNameBuffer,
		GENERIC_WRITE,
		0,                         // No sharing
		lpPipeAttributes,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | dwWriteMode,
		NULL                       // Template file
	);

	if (INVALID_HANDLE_VALUE == WritePipeHandle) {
		dwError = GetLastError();
		CloseHandle(ReadPipeHandle);
		SetLastError(dwError);
		return FALSE;
	}

	*lpReadPipe = ReadPipeHandle;
	*lpWritePipe = WritePipeHandle;
	return(TRUE);
}

DEF_CancelIoEx* CPipe::_CancelIoEx = NULL;

CPipe::CPipe()
{
	static BOOL bAPIInited = FALSE;
	if (!bAPIInited && !_CancelIoEx) {
		bAPIInited = TRUE;
		HMODULE hKernel32 = GetModuleHandleA("kernel32");
		*(void**)&_CancelIoEx = GetProcAddress(hKernel32, "CancelIoEx");
	}
	m_h = NULL;
	memset(&OverlappedWrite, 0, sizeof(OverlappedWrite));
	memset(&OverlappedRead, 0, sizeof(OverlappedRead));
	OverlappedWrite.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	OverlappedRead.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	m_totalWritten = m_totalRead = 0;
	m_bCompatibleWithOlderOS = TRUE;
}

CPipe::~CPipe()
{
	Close();
}

void CPipe::Attach(HANDLE h)
{
	assert(m_h == NULL);
	m_h = h;
}

HANDLE CPipe::Detach()
{
	HANDLE h = m_h;
	m_h = NULL;
	return h;
}


BOOL CPipe::CompatibleWithOlderOS(BOOL b)
{
	if (!b) {
		if (!_CancelIoEx) {
			return FALSE;
		}
	}
	m_bCompatibleWithOlderOS = b;
	return TRUE;
}

int CPipe::Read(void* buf, int bufsize, DWORD timeout_ms)
{
	DWORD dwRead;

	BOOL bSucc = ReadFile(m_h, buf, bufsize, &dwRead, &OverlappedRead);
	if (!bSucc)
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			if (WAIT_TIMEOUT == WaitForSingleObject(OverlappedRead.hEvent, timeout_ms)) {
				if (m_bCompatibleWithOlderOS)
					CancelIo(m_h);
				else
					_CancelIoEx(m_h, &OverlappedRead);
				//可能取消失败，因为正要取消时就已经完成了。
				//不管取消是否成功，bWait用TRUE重新再查询结果，保证OVERLAPPED完成
				bSucc = GetOverlappedResult(m_h, &OverlappedRead, &dwRead, TRUE);
				if (!bSucc)
					return TIMEOUT;
			}
			else {
				bSucc = GetOverlappedResult(m_h, &OverlappedRead, &dwRead, FALSE);
				if (!bSucc)
					return -1;
			}
		}
		else {
			return -1;
		}
	}
	m_totalRead += dwRead;
	return (int)dwRead;
}

int CPipe::Write(const void* data, int len, DWORD timeout_ms)
{
	DWORD dwWritten;
	BOOL bSucc;

	bSucc = WriteFile(m_h, data, len, &dwWritten, &OverlappedWrite);
	if (!bSucc) {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (WAIT_TIMEOUT == WaitForSingleObject(OverlappedWrite.hEvent, timeout_ms)) {
				if (m_bCompatibleWithOlderOS)
					CancelIo(m_h);
				else
					_CancelIoEx(m_h, &OverlappedWrite);
				bSucc = GetOverlappedResult(m_h, &OverlappedWrite, &dwWritten, TRUE);
				if (!bSucc)
					return TIMEOUT;
			}
			else {
				bSucc = GetOverlappedResult(m_h, &OverlappedWrite, &dwWritten, FALSE);
				if (!bSucc)
					return -1;
			}
		}
		else {
			return -1;
		}
	}
	m_totalWritten += dwWritten;
	return (int)dwWritten;
}

void CPipe::Close()
{
	if (OverlappedRead.hEvent) {
		CloseHandle(OverlappedRead.hEvent);
		OverlappedRead.hEvent = NULL;
	}
	if (OverlappedWrite.hEvent) {
		CloseHandle(OverlappedWrite.hEvent);
		OverlappedWrite.hEvent = NULL;
	}
	if (m_h) {
		ClosePipeHanle(m_h);
		m_h = NULL;
	}
}

void CPipe::GetStat(UINT64* W, UINT64* R)
{
	if (W)
		*W = m_totalWritten;
	if (R)
		*R = m_totalRead;
}

void CPipe::ClosePipeHanle(HANDLE h)
{
	::CloseHandle(h);
}
