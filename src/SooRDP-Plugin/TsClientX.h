// TsClientX.h: CTsClientX 的声明

#pragma once
#include "resource.h"       // 主符号



#include "SooRDPPlugin_i.h"
#include "../misc/memshare.h"
#include "idata.h"
#include "../misc/NBSocket.h"
#include "../misc/netutils.h"
#include "../misc/Thread.h"
#include "SooRDChannel.h"
#include <list>
#include <time.h>

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Windows CE 平台(如不提供完全 DCOM 支持的 Windows Mobile 平台)上无法正确支持单线程 COM 对象。定义 _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA 可强制 ATL 支持创建单线程 COM 对象实现并允许使用其单线程 COM 对象实现。rgs 文件中的线程模型已被设置为“Free”，原因是该模型是非 DCOM Windows CE 平台支持的唯一线程模型。"
#endif

using namespace ATL;


// CTsClientX

class ATL_NO_VTABLE CTsClientX :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<CTsClientX, &CLSID_TsClientX>,
	public IDispatchImpl<ITsClientX, &IID_ITsClientX, &LIBID_SooRDPPluginLib, /*wMajor =*/ 1, /*wMinor =*/ 0>,
	public IWTSPlugin,
	public IWTSListenerCallback
{
	_SharedMem* m_pConfig;
	bool m_flagExit;
	CComPtr<class CSampleListenerCallback> m_ptrListenerCallback_Data;
	CComPtr< CSooRDC > m_ptrRDC;


public:
	CTsClientX()
	{
		m_pConfig = NULL;
		m_flagExit = false;
	}

DECLARE_REGISTRY_RESOURCEID(106)


BEGIN_COM_MAP(CTsClientX)
	COM_INTERFACE_ENTRY(ITsClientX)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IWTSPlugin)
	COM_INTERFACE_ENTRY(IWTSListenerCallback)
END_COM_MAP()



	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct();
	void FinalRelease();

public:

	// IWTSPlugin.
	//
	HRESULT STDMETHODCALLTYPE Initialize(IWTSVirtualChannelManager* pChannelMgr);
	HRESULT STDMETHODCALLTYPE Connected();
	HRESULT STDMETHODCALLTYPE Disconnected(DWORD dwDisconnectCode);
	HRESULT STDMETHODCALLTYPE Terminated();

	//IWTSListenerCallback
	//
	HRESULT STDMETHODCALLTYPE OnNewChannelConnection(
			__in IWTSVirtualChannel* pChannel,
			__in_opt BSTR data,
			__out BOOL* pbAccept,
			__out IWTSVirtualChannelCallback** ppCallback);

};

class ATL_NO_VTABLE CSampleListenerCallback :
	public CComObjectRootEx<CComMultiThreadModel>,
	public IWTSListenerCallback
{
public:
	IWTSListenerCallback* m_pNotify;
	void* m_userdata;

	CSampleListenerCallback() {
		m_pNotify = NULL;
		m_userdata = NULL;
	}
	BEGIN_COM_MAP(CSampleListenerCallback)
		COM_INTERFACE_ENTRY(IWTSListenerCallback)
	END_COM_MAP()

	HRESULT STDMETHODCALLTYPE
		OnNewChannelConnection(
			__in IWTSVirtualChannel* pChannel,
			__in_opt BSTR data,
			__out BOOL* pbAccept,
			__out IWTSVirtualChannelCallback** ppCallback) {

		return m_pNotify->OnNewChannelConnection(pChannel, (BSTR)m_userdata, pbAccept, ppCallback);
	}
};


OBJECT_ENTRY_AUTO(__uuidof(TsClientX), CTsClientX)
