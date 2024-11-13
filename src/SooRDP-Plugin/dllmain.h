// dllmain.h: 模块类的声明。

class CSooRDPPluginModule : public ATL::CAtlDllModuleT< CSooRDPPluginModule >
{
public :
	DECLARE_LIBID(LIBID_SooRDPPluginLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_SOORDPPLUGIN, "{3f4335b8-3e53-41a9-9367-d0e9d85a2d4e}")
};

extern class CSooRDPPluginModule _AtlModule;
