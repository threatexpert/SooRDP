#include "pch.h"
#include "ProcessUtils.h"
#include <Windows.h>
#include <Tlhelp32.h>


static BOOL GetProcessNameByPid_priv(HANDLE hSP, DWORD dwPID, LPTSTR lpszName, int NameSize)
{
	BOOL ret = FALSE;
	PROCESSENTRY32W pe;
	DWORD dwRet;

	pe.dwSize = sizeof(pe);

	for (dwRet = Process32FirstW(hSP, &pe);
		dwRet;
		dwRet = Process32NextW(hSP, &pe))
	{
		if (dwPID == pe.th32ProcessID)
		{
			_tcsncpy_s(lpszName, NameSize, pe.szExeFile, NameSize);
			ret = TRUE;
		}
	}

	return ret;
}

BOOL GetParentPid(DWORD dwPID, DWORD * pPPID, LPTSTR lpszName, int NameSize)
{
	BOOL ret = FALSE;
	PROCESSENTRY32W pe;
	DWORD dwRet;

	HANDLE hSP = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSP)
	{
		pe.dwSize = sizeof(pe);

		for (dwRet = Process32FirstW(hSP, &pe);
			dwRet;
			dwRet = Process32NextW(hSP, &pe))
		{
			if (dwPID == pe.th32ProcessID)
			{
				*pPPID = pe.th32ParentProcessID;
				if (lpszName) {
					lpszName[0] = '\0';
					GetProcessNameByPid_priv(hSP, pe.th32ParentProcessID, lpszName, NameSize);
				}
				ret = TRUE;
			}
		}
		::CloseHandle(hSP);
	}

	return ret;
}

typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

static LPFN_ISWOW64PROCESS fnIsWow64Process = nullptr;

BOOL __IsWow64Process(HANDLE hProc)
{
	BOOL bIsWow64 = FALSE;

	if (!fnIsWow64Process) {
		fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
			GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
	}

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(hProc, &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}

BOOL IsWow64Process(DWORD dwPID)
{
	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPID);
	if (!hProc)
		return FALSE;

	BOOL bIsWow64 = __IsWow64Process(hProc);
	CloseHandle(hProc);
	return bIsWow64;
}

IntegrityLevel GetProcessIntegrityLevel(HANDLE hProc) {
	HANDLE process_token = nullptr;
	OpenProcessToken(hProc, TOKEN_QUERY, &process_token);

	DWORD token_info_length = 0;
	if (::GetTokenInformation(process_token, TokenIntegrityLevel,
		nullptr, 0, &token_info_length) ||
		::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
		return INTEGRITY_UNKNOWN;
	}

	IntegrityLevel ret = INTEGRITY_UNKNOWN;
	BYTE* token_label_bytes = new BYTE[token_info_length];
	TOKEN_MANDATORY_LABEL* token_label =
		reinterpret_cast<TOKEN_MANDATORY_LABEL*>(token_label_bytes);

	do
	{
		if (!::GetTokenInformation(process_token, TokenIntegrityLevel,
			token_label, token_info_length,
			&token_info_length)) {
			break;
		}
		DWORD integrity_level = *::GetSidSubAuthority(
			token_label->Label.Sid,
			static_cast<DWORD>(*::GetSidSubAuthorityCount(token_label->Label.Sid) -
				1));

		if (integrity_level < SECURITY_MANDATORY_LOW_RID) {
			ret = UNTRUSTED_INTEGRITY;
			break;
		}

		if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID) {
			ret = LOW_INTEGRITY;
			break;
		}

		if (integrity_level >= SECURITY_MANDATORY_MEDIUM_RID &&
			integrity_level < SECURITY_MANDATORY_HIGH_RID) {
			ret = MEDIUM_INTEGRITY;
			break;
		}

		if (integrity_level >= SECURITY_MANDATORY_HIGH_RID) {
			ret = HIGH_INTEGRITY;
			break;
		}
	} while (0);

	delete[]token_label_bytes;
	CloseHandle(process_token);
	return ret;
}

IntegrityLevel GetProcessIntegrityLevel(DWORD dwPid)
{
	IntegrityLevel ret = INTEGRITY_UNKNOWN;
	HANDLE hProc;
	
	hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPid);
	if (!hProc) {
		hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwPid);
	}
	if (hProc) {
		ret = GetProcessIntegrityLevel(hProc);
		CloseHandle(hProc);
	}
	return ret;
}
