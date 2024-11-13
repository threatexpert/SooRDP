#include "pch.h"
#include "memshare.h"
#include <sddl.h>

Cmemshare::Cmemshare()
{
	m_hMapFile = NULL;
	m_pData = NULL;
}

Cmemshare::~Cmemshare()
{
	Close();
}

static
ULONG CreateSectionWithLowAccess(PHANDLE SectionHandle, ULONG dwMaximumSize, PCSTR lpName)
{
	SECURITY_ATTRIBUTES sa = { sizeof(sa) };


	if (ConvertStringSecurityDescriptorToSecurityDescriptorA("D:PNO_ACCESS_CONTROLS:(ML;;NW;;;LW)",
		SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL))
	{
		*SectionHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, dwMaximumSize, lpName);

		LocalFree(sa.lpSecurityDescriptor);

		return *SectionHandle ? NOERROR : GetLastError();
	}

	return GetLastError();
}

bool Cmemshare::Create(const char* fmname, int memsize)
{
	 CreateSectionWithLowAccess(
		 &m_hMapFile,
		 memsize,                // buffer size  
		 fmname);                 // name of mapping object

	if (m_hMapFile == NULL)
	{
		m_hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, memsize, fmname);
		if (m_hMapFile == NULL)
			return false;
	}

	m_pData  = MapViewOfFile(m_hMapFile,   // handle to map object
		FILE_MAP_READ | FILE_MAP_WRITE, // read/write permission
		0,
		0,
		memsize);
	if (!m_pData)
	{
		return false;
	}

	memset(m_pData, 0, memsize);

	return true;
}

bool Cmemshare::Open(const char* fmname, int memsize)
{
	m_hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, fmname);
	if (m_hMapFile == NULL)
	{
		return false;
	}

	m_pData = MapViewOfFile(m_hMapFile,   // handle to map object
		FILE_MAP_READ | FILE_MAP_WRITE, // read/write permission
		0,
		0,
		memsize);
	if (!m_pData)
	{
		return false;
	}

	return true;
}

void Cmemshare::Close()
{
	if (m_pData != NULL)
	{
		::UnmapViewOfFile(m_pData);
		m_pData = NULL;
	}

	if (m_hMapFile != NULL)
	{
		::CloseHandle(m_hMapFile);
		m_hMapFile = NULL;
	}
}

void* Cmemshare::GetMem()
{
	return m_pData;
}
