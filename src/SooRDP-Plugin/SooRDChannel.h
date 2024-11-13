#pragma once
#include "resource.h"       // ������

#include "SooRDPPlugin_i.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE ƽ̨(�粻�ṩ��ȫ DCOM ֧�ֵ� Windows Mobile ƽ̨)���޷���ȷ֧�ֵ��߳� COM ���󡣶��� _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA ��ǿ�� ATL ֧�ִ������߳� COM ����ʵ�ֲ�����ʹ���䵥�߳� COM ����ʵ�֡�rgs �ļ��е��߳�ģ���ѱ�����Ϊ��Free����ԭ���Ǹ�ģ���Ƿ� DCOM Windows CE ƽ̨֧�ֵ�Ψһ�߳�ģ�͡�"
#endif

#include "../misc/Pipe.h"
#include "../misc/SoTunnel.h"
#include "../misc/Thread.h"

using namespace ATL;


class CSoTunnelRDC : public CSoTunnel
{
public:
	CComPtr<IWTSVirtualChannel> m_ptrChannel;

	virtual void OnChannelSocketReady(bool ok);
	virtual void OnChannelSocketClose();
	virtual void OnChannelPingTimeout();
};

class ATL_NO_VTABLE CSooRDC :
	public CComObjectRootEx<CComMultiThreadModel>,
	public IWTSVirtualChannelCallback
{
	CComPtr<IWTSVirtualChannel> m_ptrChannel;
	BOOL m_bChannelCloseFlag;
	__int64 m_totalReceived;
	CSoTunnelRDC* m_pSoTunnel;
	class CSotunnel2TsChannel * m_pS2tSocket;
public:

	BEGIN_COM_MAP(CSooRDC)
		COM_INTERFACE_ENTRY(IWTSVirtualChannelCallback)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()
	HRESULT FinalConstruct();

	void FinalRelease();

	BOOL SetChannel(IWTSVirtualChannel* pChannel);

	// IWTSVirtualChannelCallback
	//
	HRESULT STDMETHODCALLTYPE OnDataReceived(
		/* [in] */ ULONG cbSize,
		/* [size_is][in] */ __RPC__in_ecount_full(cbSize) BYTE* pBuffer);

	HRESULT STDMETHODCALLTYPE OnClose();

	void CloseChannel();

};

class CSotunnel2TsChannel
	: public Casocket
{
	enum {
		bufsize = 1024 * 1024
	};
	xbuf* m_readbuf;

public:
	CComPtr<IWTSVirtualChannel> m_ptrChannel;
	CSotunnel2TsChannel();
	~CSotunnel2TsChannel();
	virtual void OnRead(int err);
	virtual void OnClose(int err);
	virtual void OnRelease();
	void CloseChannel();
};
