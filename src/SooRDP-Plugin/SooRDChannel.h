#pragma once
#include "resource.h"       // 主符号

#include "SooRDPPlugin_i.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
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
