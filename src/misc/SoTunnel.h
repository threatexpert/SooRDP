#pragma once
#include "ASocket.h"
#include "xbuf.h"
#include <string>
#include <set>
#include <time.h>
#include "Thread.h"
#include "NBSocket.h"

#pragma pack(push, 1)
struct stblkhdr
{
    unsigned char cmd;
    unsigned int id;
    unsigned short len;
};

#define stconfig_magic    0x41424344
#define stmode_redir      1
#define stmode_socks5     2

struct st_config
{
    unsigned long magic;
    unsigned char mode;
    char ip[64];
    unsigned short port;
};

struct st_ack
{
    uint64_t recv;
    uint32_t remaining;
};

#pragma pack(pop)

#define stb_cmd_newconn 0
#define stb_cmd_data    1
#define stb_cmd_close   2
#define stb_cmd_config  3
#define stb_cmd_reset   4
#define stb_cmd_ready   5
#define stb_cmd_ruready 6
#define stb_cmd_ack     9

#define ChnMaxChunckSize (1024*60)
#define MaxBlockszPerSend  (1024*64)
#define ChnSocketBufferSize (1024*256)
#define ChnMaxPendingSize (1024*256)


class ISoTunnelCallback {
public:
    virtual void OnChannelSocketReady(bool ok) {};
    virtual void OnChannelSocketClose() {};
    virtual void OnChannelSocketCount(int count) {};
    virtual void OnChannelConfigChanged(st_config *config) {};
    virtual void OnChannelSocks5ConnectTarget(const char* host, int port) {};
    virtual void OnChannelPingTimeout() {};
};

class CChannelSocket
    : public Casocket
{
    CASocketMgr* m_pSockMgr;
    ASCONN m_connidmap;
    int m_seqIds;
    BOOL m_ack_need_update;
    CMyCriticalSection m_lc_ctrldata;
    DWORD m_last_recv;
public:
    ISoTunnelCallback* m_pCB;
    BOOL m_bReadAble;
    xbuf* m_readbuf;
    xbuf* m_writebuf;
    BOOL m_bRemoteSide;
    std::set<class CEndPointSocket*> m_readable_pp;
    st_config m_config;
    std::string m_ctrldata;
    int m_keepalive_timeout_sec;
    uint64_t m_peer_total_read;
    uint32_t m_peer_readbuf_remaining;
    uint64_t m_total_read, m_total_written;
    uint32_t m_ChnMaxPendingSize;

    CChannelSocket(CASocketMgr* pSockMgr, int BufferSize = ChnSocketBufferSize);
    ~CChannelSocket();

    BOOL Init();
    virtual void Close();
    virtual void OnTimer(int id);
    virtual void OnConnect(int err);
    virtual void OnRead(int err);
    virtual void OnWrite(int err);
    virtual void OnClose(int err);
    virtual void OnRelease();

    virtual bool OnGetReady();

    void SetCallback(ISoTunnelCallback* pCB) { m_pCB = pCB; };
    bool OnEndpointConnect(int connid);
    int OnEndPointAccept(SOCKET sClient);
    void CloseConnId(int connid);
    void OnDeleteClient(CEndPointSocket* pIns);

    int GenUid();

    void flushCtrlData();
    void ctrl_new_conn_id(int id);
    void ctrl_closing_conn_id(int id);
    void ctrl_config(unsigned char mode, const char* ip, unsigned short port);
    void ctrl_reset();
    void ctrl_ready();
    void ctrl_askready();
    void EnableKeepalive(int timeout_value_sec);
    void ctrl_ack();

    int ParseAndDeliverData();
    int dispatch(struct stblkhdr* hdr, const char* buf, int len);

};

#define EndpointLocal 0
#define EndpointRemote 1

class CEndPointSocket
    : public Casocket
{
public:
    int m_type;
    BOOL m_bReadAble;
    xbuf* m_writebuf;
    CChannelSocket* m_pChnl;
    BOOL m_closing;
    int m_id;
    BOOL m_bSayClosing;
    BOOL m_bConnecting;
    BOOL m_bSocksTalking;
    std::string    s5buf;
    int s5_stage;
    unsigned char s5_nmethod;
    CEndPointSocket(CChannelSocket* pChnlSock, int type, int BufferSize = ChnSocketBufferSize);
    virtual ~CEndPointSocket();
    int GetID() { return m_id; }

    bool HandleRedir();
    bool HandleSocks5();
    void OnSocksTalk();
    bool s5data_read(int needsize);
    bool channel_write(const void *data, int len);
    virtual void OnConnect(int err);
    virtual void OnRead(int err);
    virtual void OnWrite(int err);
    virtual void OnClose(int err);
    virtual void OnRelease();
    virtual void Close();
    virtual void OnTimer(int id);
};

class CasLocalAcceptor
    : public Casocket
{
    CChannelSocket* m_pChnSocket;
public:

    CasLocalAcceptor(CChannelSocket *pChnSocket);
    virtual ~CasLocalAcceptor();

    virtual void OnAccept(int err);
    virtual void OnClose(int err);
    virtual void OnRelease();
};

class CasTcpPair
    : public Casocket
{
    xbuf* m_writebuf;
    BOOL m_bReadAble;
    BOOL m_closing;
public:
    CasTcpPair* m_pp;
    int _persendsz;
    uint64_t m_total_read, m_total_written;
    CasTcpPair(int BufferSize = 1024*128, int persend=MaxBlockszPerSend);
    ~CasTcpPair();
    virtual void Close();
    virtual void OnRead(int err);
    virtual void OnWrite(int err);
    virtual void OnClose(int err);
    virtual void OnRelease() {};
};

class CSoTunnel
    : public CThread
    , public ISoTunnelCallback
{
    CASocketMgr m_asmgr;
    CChannelSocket* m_pChnSocket;
    CasLocalAcceptor* m_pAcceptor;
    HANDLE m_hInitEvent;
    int m_iMainThreadInitResult;
    int m_keepalive_timeout_second;
    uint32_t m_chn_max_pending_size;
public:
    std::wstring m_lasterr;
    Cnbsocket* m_pTunnelIO;

    CSoTunnel();
    ~CSoTunnel();

    void EnableKeepalive(int timeout_second);
    void SetChannelMaxPendingSize(uint32_t size);
    BOOL InitLocal(int bindPort, const char* bindIP="127.0.0.1");
    BOOL InitRemote();
    void Deinit();
    BOOL AddASocket(Casocket* as, long lEvent);
    BOOL SendConfig(unsigned char mode, const char* ip, unsigned short port);
    BOOL SendReset();
    BOOL SendRUReady();
protected:
    virtual DWORD run();
    virtual void OnChannelSocketClose();

};