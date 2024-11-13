#pragma once



LPCWSTR LSTRW(int i);

enum {
	RID_UNK,
	RID_TunnelNotReady,
	RID_TunnelReady,
	RID_TunnelKeepaliveTimeout,
	RID_TunnelDataChannelClosed,
	RID_SVCSocketClosed,
	RID_SVCTimeoutClose,
	RID_LocallyOpenSVCTunnel,
	RID_LocallyOpenSVCTunnelFailed,
	RID_AskingMakeSVCTunnel,
	RID_AskingMakeDVCTunnel,
	RID_TunnelErr_closing,
	RID_Tunnel_Init_Done,
	RID_Tunnel_Init_Err,
	RID_Process_PID,
	RID_InitPlugFaided,
	RID_InitPlugDone,
	RID_RDPConnected,
	RID_RDPDisconnected,
	RID_RDPTerminated,
	RID_ClosePrevTunnel,
	RID_AskingMakeChannelFailed,
	RID_InitDVCTunnelDone,
};