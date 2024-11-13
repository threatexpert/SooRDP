#pragma once


BOOL GetParentPid(DWORD dwPID, DWORD *pPPID, LPTSTR lpszName, int NameSize);
BOOL IsWow64Process(DWORD dwPID);
BOOL __IsWow64Process(HANDLE hProc);

enum IntegrityLevel {
	INTEGRITY_UNKNOWN,
	UNTRUSTED_INTEGRITY,
	LOW_INTEGRITY,
	MEDIUM_INTEGRITY,
	HIGH_INTEGRITY
};
IntegrityLevel GetProcessIntegrityLevel(HANDLE hProc);
IntegrityLevel GetProcessIntegrityLevel(DWORD dwPid);
