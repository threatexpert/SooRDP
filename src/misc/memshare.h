#pragma once



class Cmemshare
{
	void *m_pData;
	HANDLE m_hMapFile;
public:
	Cmemshare();
	~Cmemshare();

	bool Create(const char* fmname, int memsize);
	bool Open(const char* fmname, int memsize);
	void Close();
	void* GetMem();

};

