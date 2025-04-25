#include "pch.h"
#include "SoTunnel.h"
#include "netutils.h"

#ifdef _DEBUG
#define Dbg  ATLTRACE
#else
#define Dbg(...)
#endif

#define TM_WAIT_READY  1
#define TM_WAIT_CONFIG 2
#define TM_DELAY_CLOSE 3
#define TM_KEEPALIVE   5
#define TM_ACK         6

CChannelSocket::CChannelSocket(CASocketMgr* pSockMgr, int BufferSize )
{
    m_readbuf = xbuf_create(BufferSize);
    m_writebuf = xbuf_create(BufferSize);
    m_pSockMgr = pSockMgr;
    m_bReadAble = FALSE;
    m_seqIds = 0;
    memset(&m_config, 0, sizeof(m_config));
    m_pCB = NULL;
    m_total_read = m_total_written = m_peer_total_read = m_peer_readbuf_remaining = 0;
    m_bRemoteSide = FALSE;
    m_keepalive_timeout_sec = 0;
    m_last_recv = GetTickCount();
    m_ack_need_update = FALSE;
    m_ChnMaxPendingSize = ChnMaxPendingSize;
}

CChannelSocket::~CChannelSocket()
{
    xbuf_free(m_readbuf);
    xbuf_free(m_writebuf);
}

void CChannelSocket::OnRead(int err)
{
    if (err != 0) {
        Close();
        return;
    }
    ParseAndDeliverData();
    if (m_readbuf->datalen < ChnMaxChunckSize + sizeof(stblkhdr)) {
        xbuf_ensureavail(m_readbuf, ChnMaxChunckSize + sizeof(stblkhdr));
    }
    int bufavail = xbuf_avail(m_readbuf);
    if (bufavail == 0) {
        m_bReadAble = TRUE;
        return;
    }
    int ret = recv(m_hSocket, xbuf_datatail(m_readbuf), bufavail, 0);
    if (ret < 0) {
        if (WSAEWOULDBLOCK == WSAGetLastError()) {
            m_bReadAble = FALSE;
            return;
        }
        Close();
        return;
    }
    else if (ret == 0) {
        Close();
        return;
    }
    m_bReadAble = FALSE;
    m_total_read += ret;
    xbuf_appended(m_readbuf, ret);
    m_ack_need_update = TRUE;
    if (ParseAndDeliverData() == 2) {
        m_ack_need_update = FALSE;
        ctrl_ack();
        Trigger(FD_WRITE, 0);
    }
    m_last_recv = GetTickCount();
}

BOOL CChannelSocket::Init()
{
    if (m_bRemoteSide) {
        ctrl_ready();
        setTimer(TM_WAIT_CONFIG, 20000);
        Select(FD_READ | FD_WRITE | FD_CLOSE);
        Trigger(FD_READ, 0);
        Trigger(FD_WRITE, 0);
    }
    else {
        setTimer(TM_WAIT_READY, 20000);
        Select(FD_READ | FD_CLOSE);
        Trigger(FD_READ, 0);
    }
    if (m_keepalive_timeout_sec) {
        setTimer(TM_KEEPALIVE, 1000);
    }
    setTimer(TM_ACK, 1000);
    return TRUE;
}

void CChannelSocket::Close()
{
    if (m_hSocket != INVALID_SOCKET) {
        if (m_pCB) {
            m_pCB->OnChannelSocketClose();
        }
    }
    Casocket::Close();
}

void CChannelSocket::OnTimer(int id)
{
    if (id == TM_WAIT_READY) {
        killTimer(TM_WAIT_READY);
        setTimer(TM_DELAY_CLOSE, 2000);
        if (m_pCB) {
            m_pCB->OnChannelSocketReady(false);
        }
    }
    else if (id == TM_WAIT_CONFIG) {
        killTimer(TM_WAIT_CONFIG);
        setTimer(TM_DELAY_CLOSE, 2000);
        if (m_pCB) {
            m_pCB->OnChannelSocketReady(false);
        }
    }
    else if (id == TM_DELAY_CLOSE) {
        killTimer(TM_DELAY_CLOSE);
        Trigger(FD_CLOSE, -2);
    }
    else if (id == TM_KEEPALIVE) {
        if (m_keepalive_timeout_sec > 0) {
            DWORD now = GetTickCount();
            if (now - m_last_recv > (DWORD)m_keepalive_timeout_sec*1000) {
                setTimer(TM_DELAY_CLOSE, 2000);
                if (m_pCB) {
                    m_pCB->OnChannelPingTimeout();
                }
            }
        }
    }
    else if (id == TM_ACK) {
        if (m_ack_need_update) {
            m_ack_need_update = FALSE;
            ctrl_ack();
            Trigger(FD_WRITE, 0);
        }
    }
}

void CChannelSocket::OnConnect(int err)
{
    OnClose(err);
}

int CChannelSocket::GenUid()
{
    return ++m_seqIds;
}

int CChannelSocket::ParseAndDeliverData()
{
    int ret = 0;
    stblkhdr hdr;
    int pos = 0;
    int datasz = 0;
    while (m_readbuf->datalen - pos >= sizeof(stblkhdr)) {
        memcpy(&hdr, &m_readbuf->data[m_readbuf->datapos + pos], sizeof(hdr));
        hdr.id = ntohl(hdr.id);
        hdr.len = ntohs(hdr.len);
        if (m_readbuf->datalen - pos - sizeof(stblkhdr) >= hdr.len) {
            ret = dispatch(&hdr, &m_readbuf->data[m_readbuf->datapos + pos + sizeof(stblkhdr)], hdr.len);
            if (ret == 0) {
                break;
            }
            else if (ret == -2) {
                Trigger(FD_CLOSE, -2);
                return -2;
            }
            else {
                pos += sizeof(stblkhdr) + hdr.len;
                if (hdr.cmd == stb_cmd_data) {
                    datasz += hdr.len;
                }
            }
        }
        else {
            break;
        }
    }
    if (pos) {
        xbuf_pos_forward(m_readbuf, pos);
        if (m_readbuf->datalen) {
            m_bReadAble = TRUE;
        }
    }
    if (datasz != 0) {
        ret = 2;
    }
    return ret;
}

int CChannelSocket::dispatch(struct stblkhdr* hdr, const char* buf, int len)
{
    if (hdr->cmd == stb_cmd_data && len != 0) {
        //Dbg("DeliverData:: id=%d, cmd=data\n", hdr->id);
        ASCONN::iterator it = m_connidmap.find(hdr->id);
        if (it == m_connidmap.end()) {
            return -1;
        }
        CEndPointSocket* pSock = (CEndPointSocket*)it->second;
        if (!xbuf_append(pSock->m_writebuf, buf, len)) {
            return 0;
        }
        pSock->OnWrite(0);
        return 1;
    }
    else if (hdr->cmd == stb_cmd_newconn && len == 0) {
        Dbg("DeliverData:: id=%d, cmd=new-conn\n", hdr->id);
        if (!OnEndpointConnect(hdr->id)) {
            return -1;
        }
        return 1;
    }
    else if (hdr->cmd == stb_cmd_close && len == 0) {
        Dbg("DeliverData:: id=%d, cmd=closing-conn\n", hdr->id);
        ASCONN::iterator it = m_connidmap.find(hdr->id);
        if (it == m_connidmap.end()) {
            return -1;
        }
        CEndPointSocket* pSock = (CEndPointSocket*)it->second;
        if (pSock->m_writebuf->datalen) {
            pSock->m_closing = TRUE;
        }
        else {
            pSock->Close();
        }
        return 1;
    }
    else if (hdr->cmd == stb_cmd_config && len == sizeof(st_config)) {
        st_config* cfg = (st_config*)buf;
        if (stconfig_magic == ntohl(cfg->magic) && (cfg->mode == stmode_redir || cfg->mode == stmode_socks5)) {
            memcpy(&m_config, buf, len);
            m_config.magic = ntohl(m_config.magic);
            m_config.port = ntohs(m_config.port);
            killTimer(TM_WAIT_CONFIG);
            if (m_pCB) {
                m_pCB->OnChannelConfigChanged(&m_config);
            }
            return 1;
        }
        return -2;
    }
    else if (hdr->cmd == stb_cmd_reset && len == 0) {
        return -2;
    }
    else if (hdr->cmd == stb_cmd_ready && len == 0) {
        if (!OnGetReady()) {
            return -2;
        }
        return 1;
    }
    else if (hdr->cmd == stb_cmd_ruready && len == 0) {
        ctrl_ready();
        Trigger(FD_WRITE, 0);
        return 1;
    }
    else if (hdr->cmd == stb_cmd_ack && len == sizeof(st_ack)) {
        st_ack* c = (st_ack*)buf;
        m_peer_total_read = _ntohll(c->recv);
        m_peer_readbuf_remaining = ntohl(c->remaining);
        Trigger(FD_WRITE, 0);
        Dbg("0x%p got a ack, peer-recv=%I64d, peer-remaining=%d\n", this, m_peer_total_read, m_peer_readbuf_remaining);
        return 1;
    }
    else {
        Dbg("DeliverData:: ERR, id=%d, cmd=%d, len=%d\n", hdr->id, (int)hdr->cmd, (int)hdr->len);
        return -1;
    }
}

void CChannelSocket::flushCtrlData()
{
    CAutoCriticalSection lc(m_lc_ctrldata);

    while (m_ctrldata.size()) {
        stblkhdr* hdr = (stblkhdr*)&m_ctrldata[0];
        int bs = sizeof(stblkhdr) + ntohs(hdr->len);
        if (!xbuf_ensureavail(m_writebuf, bs))
            break;
        if (!xbuf_append(m_writebuf, (char*)&m_ctrldata[0], bs))
            break;
        m_ctrldata.erase(0, bs);
    }
}

void CChannelSocket::ctrl_new_conn_id(int id)
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    stblkhdr hdr;
    hdr.cmd = stb_cmd_newconn;
    hdr.id = htonl(id);
    hdr.len = 0;
    m_ctrldata.append((char*)&hdr, sizeof(hdr));
}

void CChannelSocket::ctrl_closing_conn_id(int id)
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    stblkhdr hdr;
    hdr.cmd = stb_cmd_close;
    hdr.id = htonl(id);
    hdr.len = 0;
    m_ctrldata.append((char*)&hdr, sizeof(hdr));
}

void CChannelSocket::ctrl_config(unsigned char mode, const char *ip, unsigned short port)
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    char buf[sizeof(stblkhdr) + sizeof(st_config)];
    stblkhdr* hdr = (stblkhdr*)&buf[0];
    st_config* cfg = (st_config*)&buf[sizeof(stblkhdr)];

    hdr->cmd = stb_cmd_config;
    hdr->id = 0;
    hdr->len = htons(sizeof(st_config));
    cfg->magic = htonl(stconfig_magic);
    cfg->mode = mode;
    strcpy_s(cfg->ip, ip);
    cfg->port = htons(port);
    m_ctrldata.append(buf, sizeof(stblkhdr) + sizeof(st_config));
}

void CChannelSocket::ctrl_reset()
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    stblkhdr hdr;
    hdr.cmd = stb_cmd_reset;
    hdr.id = 0;
    hdr.len = 0;
    m_ctrldata.append((char*)&hdr, sizeof(hdr));
}

void CChannelSocket::ctrl_ready()
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    stblkhdr hdr;
    hdr.cmd = stb_cmd_ready;
    hdr.id = 0;
    hdr.len = 0;
    m_ctrldata.append((char*)&hdr, sizeof(hdr));
}

void CChannelSocket::ctrl_askready()
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    stblkhdr hdr;
    hdr.cmd = stb_cmd_ruready;
    hdr.id = 0;
    hdr.len = 0;
    m_ctrldata.append((char*)&hdr, sizeof(hdr));
}

void CChannelSocket::EnableKeepalive(int timeout_value_sec)
{
    m_keepalive_timeout_sec = timeout_value_sec;
}

void CChannelSocket::ctrl_ack()
{
    CAutoCriticalSection lc(m_lc_ctrldata);
    char buf[sizeof(stblkhdr) + sizeof(st_ack)];
    stblkhdr* hdr = (stblkhdr*)&buf[0];
    st_ack* c = (st_ack*)&buf[sizeof(stblkhdr)];

    hdr->cmd = stb_cmd_ack;
    hdr->id = 0;
    hdr->len = htons(sizeof(st_ack));
    c->recv = _htonll(m_total_read);
    c->remaining = htonl(m_readbuf->datalen);
    m_ctrldata.append(buf, sizeof(stblkhdr) + sizeof(st_ack));
}

void CChannelSocket::OnWrite(int err)
{
    if (err != 0) {
        Close();
        return;
    }
    int ret;
    int sentsz = 0;
    int blocksz;
    flushCtrlData();

    while (m_writebuf->datalen) {
        blocksz = min(MaxBlockszPerSend, m_writebuf->datalen);
        ret = send(m_hSocket, xbuf_data(m_writebuf), blocksz, 0);
        if (ret < 0) {
            if (WSAEWOULDBLOCK == WSAGetLastError()) {
                break;
            }
            Close();
            return;
        }
        else if (ret == 0) {
            Close();
            return;
        }
        else {
            sentsz += ret;
            m_total_written += ret;
            xbuf_pos_forward(m_writebuf, ret);
        }
    }

    if (sentsz || !m_writebuf->datalen) {
        while (m_readable_pp.size()) {
            CEndPointSocket* pSock = *m_readable_pp.begin();
            m_readable_pp.erase(m_readable_pp.begin());
            if (pSock->m_bReadAble) {
                pSock->m_bReadAble = FALSE;
                pSock->Trigger(FD_READ, 0);
            }
        }
        flushCtrlData();
    }
}

void CChannelSocket::OnClose(int err)
{
    Dbg("CChannelSocket::OnClose(%d)\n", err);
    Close();
}

void CChannelSocket::OnRelease()
{
    Dbg("CChannelSocket::OnRelease\n");

}

bool CChannelSocket::OnGetReady()
{
    killTimer(TM_WAIT_READY);
    if (m_pCB) {
        m_pCB->OnChannelSocketReady(true);
    }
    if (!Select(FD_READ | FD_WRITE | FD_CLOSE)) {
        return false;
    }
    Trigger(FD_READ, 0);
    Trigger(FD_WRITE, 0);
    return true;
}

bool CChannelSocket::OnEndpointConnect(int connid)
{
    Dbg("CChannelSocket::OnEndpointConnect id=%d\n", connid);
    if (!m_bRemoteSide) {
        assert(false);
        return false;
    }
    if (m_config.mode == stmode_redir) {
        CEndPointSocket* as = new CEndPointSocket(this, EndpointRemote);
        as->m_id = connid;
        as->m_bSayClosing = TRUE;
        m_pSockMgr->AddingSocket(as, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE);
        if (!as->ConnectEx(m_config.ip, m_config.port)) {
            Dbg("CChannelSocket::OnEndpointConnect id=%d, ERROR:Connect\n", connid);
            delete as;
            return false;
        }
        as->HandleRedir();
        socket_setbufsize(as->m_hSocket, ChnSocketBufferSize);
        as->m_bReleaseOnClose = TRUE;
        m_connidmap[connid] = as;
        if (m_pCB) {
            m_pCB->OnChannelSocketCount((int)m_connidmap.size());
        }
        return true;
    }
    else if (m_config.mode == stmode_socks5) {
        CEndPointSocket* as = new CEndPointSocket(this, EndpointRemote);
        as->m_id = connid;
        as->m_bSayClosing = TRUE;
        m_pSockMgr->AddingSocket(as, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE);
        if (!as->HandleSocks5()) {
            Dbg("CChannelSocket::OnEndpointConnect id=%d, ERROR:HandleSocks5\n", connid);
            delete as;
            return false;
        }
        socket_setbufsize(as->m_hSocket, ChnSocketBufferSize);
        as->m_bReleaseOnClose = TRUE;
        m_connidmap[connid] = as;
        if (m_pCB) {
            m_pCB->OnChannelSocketCount((int)m_connidmap.size());
        }
        return true;
    }
    else {
        return false;
    }
}

int CChannelSocket::OnEndPointAccept(SOCKET sClient)
{
    if (m_bRemoteSide) {
        closesocket(sClient);
        assert(false);
        return false;
    }
    int id = GenUid();
    Dbg("CChannelSocket::OnEndPointAccept uid=%d\n", id);
    CEndPointSocket* as = new CEndPointSocket(this, EndpointLocal);
    as->m_id = id;
    as->Attach(sClient);
    socket_setbufsize(sClient, ChnSocketBufferSize);

    if (!m_pSockMgr->AddSocket(as, FD_READ | FD_WRITE | FD_CLOSE)) {
        as->m_id = 0;
        delete as;
        closesocket(sClient);
        return 0;
    }
    as->m_bReleaseOnClose = TRUE;
    m_connidmap[id] = as;
    if (m_pCB) {
        m_pCB->OnChannelSocketCount((int)m_connidmap.size());
    }
    as->m_bSayClosing = TRUE;
    ctrl_new_conn_id(id);
    OnWrite(0);
    return id;
}

void CChannelSocket::CloseConnId(int connid)
{
    ctrl_closing_conn_id(connid);
    Trigger(FD_WRITE, 0);
}

void CChannelSocket::OnDeleteClient(CEndPointSocket* pIns)
{
    m_connidmap.erase(pIns->GetID());
    m_readable_pp.erase(pIns);
    delete pIns;
    if (m_pCB) {
        m_pCB->OnChannelSocketCount((int)m_connidmap.size());
    }
}


///

CEndPointSocket::CEndPointSocket(CChannelSocket* pChnlSock, int type, int BufferSize)
{
    m_type = type;
    m_pChnl = pChnlSock;
    m_bReadAble = FALSE;
    m_writebuf = xbuf_create(BufferSize);
    m_id = 0;
    m_closing = FALSE;
    m_bConnecting = FALSE;
    m_bSocksTalking = FALSE;
    s5_stage = 0;
    m_bSayClosing = FALSE;
}

CEndPointSocket::~CEndPointSocket()
{
    xbuf_free(m_writebuf);
}


void CEndPointSocket::OnRead(int err) {
    if (err != 0) {
        Close();
        return;
    }
    assert(ChnMaxChunckSize + sizeof(stblkhdr) <= m_pChnl->m_writebuf->capacity);
    assert(ChnMaxChunckSize <= m_writebuf->capacity);
    if (m_pChnl->m_peer_readbuf_remaining >= m_pChnl->m_ChnMaxPendingSize) {
        m_bReadAble = TRUE;
        m_pChnl->m_readable_pp.insert(this);
        return;
    }
    if (m_pChnl->m_total_written - m_pChnl->m_peer_total_read >= m_pChnl->m_ChnMaxPendingSize) {
        m_bReadAble = TRUE;
        m_pChnl->m_readable_pp.insert(this);
        return;
    }
    if (m_pChnl->m_writebuf->datalen >= 10 * ChnMaxChunckSize) {
        m_bReadAble = TRUE;
        m_pChnl->m_readable_pp.insert(this);
        return;
    }
    if (!xbuf_ensureavail(m_pChnl->m_writebuf, ChnMaxChunckSize + sizeof(stblkhdr))) {
        m_bReadAble = TRUE;
        m_pChnl->m_readable_pp.insert(this);
        return;
    }
    int bufavail = xbuf_avail(m_pChnl->m_writebuf);
    int ret = recv(m_hSocket, xbuf_datatail(m_pChnl->m_writebuf) + sizeof(stblkhdr), min(ChnMaxChunckSize, bufavail - sizeof(stblkhdr)), 0);
    if (ret < 0) {
        if (WSAEWOULDBLOCK == WSAGetLastError()) {
            m_bReadAble = FALSE;
            return;
        }
        Close();
        return;
    }
    else if (ret == 0) {
        Close();
        return;
    }

    if (m_closing) {
        m_bReadAble = TRUE;
        m_pChnl->m_readable_pp.insert(this);
    }
    else {
        m_bReadAble = FALSE;
    }

    stblkhdr* hdr = (stblkhdr*)xbuf_datatail(m_pChnl->m_writebuf);
    hdr->cmd = stb_cmd_data;
    hdr->id = htonl(GetID());
    assert(ret <= ChnMaxChunckSize);
    hdr->len = htons((WORD)ret);
    xbuf_appended(m_pChnl->m_writebuf, sizeof(*hdr) + ret);
    m_pChnl->OnWrite(0);
}

void CEndPointSocket::OnWrite(int err) {
    int ret;
    int blocksz;
    int sentsz = 0;
    if (err != 0) {
        Close();
        return;
    }

    if (m_bSocksTalking) {
        OnSocksTalk();
        return;
    }

    if (m_bConnecting) {
        return;
    }

    while (m_writebuf->datalen) {
        blocksz = min(MaxBlockszPerSend, m_writebuf->datalen);
        ret = send(m_hSocket, xbuf_data(m_writebuf), blocksz, 0);
        if (ret < 0) {
            if (WSAEWOULDBLOCK == WSAGetLastError()) {
                break;
            }
            Close();
            return;
        }
        else if (ret == 0) {
            Close();
            return;
        }
        else {
            sentsz += ret;
            xbuf_pos_forward(m_writebuf, ret);
        }
    }

    if ((sentsz || !m_writebuf->datalen) && m_pChnl->m_bReadAble) {
        m_pChnl->Trigger(FD_READ, 0);
        m_pChnl->m_bReadAble = FALSE;
    }

    if (m_writebuf->datalen == 0) {
        if (m_closing)
            Close();
    }
}

bool CEndPointSocket::HandleRedir()
{
    m_bConnecting = TRUE;
    return true;
}

bool CEndPointSocket::HandleSocks5()
{
    m_bSocksTalking = TRUE;
    m_bConnecting = TRUE;
    return true;
}


static int GetSocksProxyAddress(BYTE ATYP, char* pData, int len, char* pszHost, int pHostSize, WORD* pwPort)
{
    int ret = 0;
    if (ATYP == 1 && len >= 6)//ip
    {
        //if (inet_ntop(AF_INET, pData, pszHost, pHostSize) == NULL)
        //    return 0;
        struct in_addr paddr;
        paddr.S_un.S_addr = *(uint32_t*)pData;
        strcpy_s(pszHost, pHostSize, inet_ntoa(paddr));
        *pwPort = ntohs(*(uint16_t*)&pData[4]);
        ret = 6;
        return ret;
    }
    else if (ATYP == 4 && len >= 18)//ipv6
    {
        return 0;
        //*pwPort = ntohs(*(uint16_t*)&pData[16]);
        //ret = 18;
        //return ret;
    }
    else if (ATYP == 3 && len >= (1 + (BYTE)pData[0] + 2))//域名
    {
        ret = min((BYTE)pData[0], pHostSize - 1);
        memcpy(pszHost, &pData[1], ret);
        pszHost[ret] = '\0';
        *pwPort = ntohs(*(uint16_t*)&pData[1 + ret]);

        ret += 1 + 2;//再加上第1字节的长度， 和最后端口的2字节
        return ret;
    }
    else
        return 0;

}

void CEndPointSocket::OnSocksTalk()
{
    int ret;
    if (s5_stage == 0) {
        if (!s5data_read(2)) {
            return;
        }
        if (s5buf[0] != 0x05) {
            Close();
            return;
        }
        s5_nmethod = s5buf[1];
        s5buf.erase(0, 2);
        s5_stage++;
    }
    if (s5_stage == 1) {
        if (!s5data_read(s5_nmethod))
            return;
        s5buf.erase(0, s5_nmethod);
        s5_stage++;
    }
    if (s5_stage == 2) {
        if (!channel_write("\x05\x00", 2)) {
            return;
        }
        s5_stage++;
    }
    if (s5_stage == 3) {
        if (!s5data_read(4)) {
            return;
        }
        unsigned char ver = s5buf[0];
        unsigned char cmd = s5buf[1];
        unsigned char rsv = s5buf[2];
        unsigned char address_type = s5buf[3];
        if (cmd != 1) { // tcp connect
            Close();
            return;
        }
        s5_stage++;
    }
    if (s5_stage == 4) {
        unsigned char ver = s5buf[0];
        unsigned char cmd = s5buf[1];
        unsigned char rsv = s5buf[2];
        unsigned char address_type = s5buf[3];
        int addr_len;
        if (address_type == 1) //ipv4
        {
            addr_len = 6;
            if (!s5data_read(4 + addr_len)) {
                return;
            }
        }
        else if (address_type == 3) //domain
        {
            if (!s5data_read(4 + 1)) {
                return;
            }
            unsigned char domain_len = s5buf[4];
            addr_len = 1 + domain_len + 2;
            if (!s5data_read(4 + addr_len)) {
                return;
            }
        }
        else {
            Close();
            return;
        }
        char connectIP[256] = "";
        WORD connectPort = 0;
        ret = GetSocksProxyAddress(address_type, &s5buf[4], addr_len, connectIP, _countof(connectIP), &connectPort);
        if (0 == ret) {
            Close();
            return;
        }
        s5buf.erase(0, 4 + ret);
        if (s5buf.size()) {
            assert(0);
            Close();
            return;
        }
        if (m_pChnl->m_pCB) {
            m_pChnl->m_pCB->OnChannelSocks5ConnectTarget(connectIP, connectPort);
        }
        if (!ConnectEx(connectIP, connectPort)) {
            Close();
            return;
        }
        s5_stage++;
    }
    if (s5_stage == 5) {
        //connecting
    }
    if (s5_stage == 6) {
        if (!channel_write("\x05\x00\x00\x01\x00\x00\x00\x00\x00\x00", 10))
            return;
        m_bSocksTalking = FALSE;
        Trigger(FD_WRITE, 0);
    }
}

bool CEndPointSocket::s5data_read(int needsize)
{
    while ((int)s5buf.size() < needsize && m_writebuf->datalen) {
        int left = needsize - (int)s5buf.size();
        int cp = min(m_writebuf->datalen, left);
        s5buf.append(xbuf_data(m_writebuf), cp);
        xbuf_pos_forward(m_writebuf, cp);
    }
    return (int)s5buf.size() == needsize;
}

bool CEndPointSocket::channel_write(const void* data, int len)
{
    if (!xbuf_ensureavail(m_pChnl->m_writebuf, len + sizeof(stblkhdr))) {
        return false;
    }
    assert(len <= ChnMaxChunckSize);
    memcpy(xbuf_datatail(m_pChnl->m_writebuf) + sizeof(stblkhdr), data, len);
    stblkhdr* hdr = (stblkhdr*)xbuf_datatail(m_pChnl->m_writebuf);
    hdr->cmd = stb_cmd_data;
    hdr->id = htonl(GetID());
    hdr->len = htons((WORD)len);
    xbuf_appended(m_pChnl->m_writebuf, sizeof(*hdr) + len);
    m_pChnl->Trigger(FD_WRITE, 0);
    return true;
}

void CEndPointSocket::OnConnect(int err)
{
    Dbg("CEndPointSocket::OnConnect(%d)\n", err);
    m_bConnecting = FALSE;
    if (err != 0) {
        if (m_bSocksTalking) {
            channel_write("\x05\xFF\x00\x01\x00\x00\x00\x00\x00\x00", 10);
        }
        Close();
        return;
    }
    if (m_bSocksTalking)
        s5_stage++;
    OnWrite(0);
}

void CEndPointSocket::OnClose(int err)
{
    Dbg("CEndPointSocket::OnClose(%d)\n", err);
    m_bConnecting = FALSE;
    if (err == 0) {
        if (!m_closing) {
            m_closing = TRUE;
            Trigger(FD_READ, 0);
        }
    }
    else {
        m_closing = TRUE;
        Close();
    }
}

void CEndPointSocket::OnRelease()
{
    Dbg("CEndPointSocket::OnRelease id=%d\n", GetID());
    m_pChnl->OnDeleteClient(this);
}

void CEndPointSocket::Close()
{
    if (m_bSayClosing) {
        Dbg("closing id=%d\n", m_id);
        m_pChnl->CloseConnId(m_id);
        m_bSayClosing = FALSE;
    }
    Casocket::Close();
}

void CEndPointSocket::OnTimer(int id)
{
}

/// <summary>
///
/// </summary>
CasLocalAcceptor::CasLocalAcceptor(CChannelSocket* pChnSocket)
{
    m_pChnSocket = pChnSocket;
}

CasLocalAcceptor::~CasLocalAcceptor()
{
}

void CasLocalAcceptor::OnAccept(int err)
{
    SOCKET sClient = WSAAccept(Casocket::m_hSocket, NULL, NULL, NULL, 0);
    if (sClient == INVALID_SOCKET) {
        return;
    }
    m_pChnSocket->OnEndPointAccept(sClient);
}

void CasLocalAcceptor::OnClose(int err)
{
}

void CasLocalAcceptor::OnRelease()
{
}

/////////////////////////////


CasTcpPair::CasTcpPair(int BufferSize /*= 1024 * 256*/, int persend /*= MaxBlockszPerSend*/) {
    m_writebuf = xbuf_create(BufferSize);
    m_bReadAble = FALSE;
    m_pp = NULL;
    m_closing = FALSE;
    _persendsz = persend;
    m_total_read = m_total_written = 0;
}

CasTcpPair::~CasTcpPair() {
    xbuf_free(m_writebuf);
}

void CasTcpPair::Close() {
    //Dbg("CasTcpPair::Close(this=%p)\n", this);
    if (m_pp && m_pp->m_hSocket != INVALID_SOCKET && !m_pp->m_closing) {
        m_pp->m_closing = TRUE;
        m_pp->Trigger(FD_WRITE, 0);
    }
    m_closing = TRUE;
    Casocket::Close();
}

void CasTcpPair::OnRead(int err) {
    if (err != 0) {
        Close();
        return;
    }

    int bufavail = xbuf_avail(m_pp->m_writebuf);

    if (bufavail == 0) {
        m_bReadAble = TRUE;
        return;
    }

    int ret = recv(m_hSocket, xbuf_datatail(m_pp->m_writebuf), bufavail, 0);
    if (ret < 0) {
        if (WSAEWOULDBLOCK == WSAGetLastError()) {
            m_bReadAble = FALSE;
            return;
        }
        Close();
        return;
    }
    else if (ret == 0) {
        Close();
        return;
    }
    m_bReadAble = FALSE;
    m_total_read += ret;
    xbuf_appended(m_pp->m_writebuf, ret);
    if (m_closing)
        Trigger(FD_READ, 0);
    m_pp->OnWrite(0);
}

void CasTcpPair::OnWrite(int err)
{
    if (err != 0) {
        Close();
        return;
    }
    int ret;
    int blocksz;
    int sentsz = 0;
    while (m_writebuf->datalen) {
        blocksz = min(_persendsz, m_writebuf->datalen);
        ret = send(m_hSocket, xbuf_data(m_writebuf), blocksz, 0);
        if (ret < 0) {
            if (WSAEWOULDBLOCK == WSAGetLastError()) {
                break;
            }
            Close();
            return;
        }
        else if (ret == 0) {
            Close();
            return;
        }
        else {
            sentsz += ret;
            m_total_written += ret;
            xbuf_pos_forward(m_writebuf, ret);
        }
    }
    if (sentsz || !m_writebuf->datalen) {
        if (m_pp->m_bReadAble) {
            m_pp->m_bReadAble = FALSE;
            m_pp->Trigger(FD_READ, 0);
        }
    }

    if (m_writebuf->datalen == 0) {
        if (m_closing)
            Close();
    }
}
void CasTcpPair::OnClose(int err) {
    Dbg("CasTcpPair::OnClose(this=%p) %d\n", this, err);
    if (err == 0) {
        if (!m_closing) {
            m_closing = TRUE;
            Trigger(FD_READ, 0);
        }
    }
    else {
        Close();
    }
}


/////////////////////////////


CSoTunnel::CSoTunnel()
{
    m_pChnSocket = NULL;
    m_pAcceptor = NULL;
    m_pTunnelIO = NULL;
    m_hInitEvent = NULL;
    m_iMainThreadInitResult = 0;
    m_keepalive_timeout_second = 0;
    m_chn_max_pending_size = ChnMaxPendingSize;
}

CSoTunnel::~CSoTunnel()
{
    Deinit();
}

void CSoTunnel::EnableKeepalive(int timeout_second)
{
    m_keepalive_timeout_second = timeout_second;
}

void CSoTunnel::SetChannelMaxPendingSize(uint32_t size)
{
    m_chn_max_pending_size = size;
}

BOOL CSoTunnel::InitLocal(int bindPort, const char* bindIP /*= "127.0.0.1*/)
{
    m_pChnSocket = new CChannelSocket(&m_asmgr);
    m_pChnSocket->SetCallback(this);
    m_pChnSocket->m_keepalive_timeout_sec = m_keepalive_timeout_second;
    m_pChnSocket->m_ChnMaxPendingSize = m_chn_max_pending_size;
    m_pAcceptor = new CasLocalAcceptor(m_pChnSocket);
    m_pTunnelIO = Cnbsocket::createInstance();
    
    m_pAcceptor->m_hSocket = socket_createListener(inet_addr(bindIP), bindPort, 64, false, false);
    if (m_pAcceptor->m_hSocket == INVALID_SOCKET) {
        m_lasterr = L"监听端口失败";
        return FALSE;
    }
    SOCKET tunnelSocketIO;
    if (!socket_mkpipes(&m_pChnSocket->m_hSocket, &tunnelSocketIO, true)) {
        m_lasterr = L"初始化管道失败";
        return FALSE;
    }
    socket_setbufsize(m_pChnSocket->m_hSocket, ChnSocketBufferSize);
    socket_setbufsize(tunnelSocketIO, ChnSocketBufferSize);
    m_pTunnelIO->Attach(tunnelSocketIO);
    m_hInitEvent = CreateEvent(0, 1, 0, 0);
    CThread::Start();
    WaitForSingleObject(m_hInitEvent, INFINITE);
    CloseHandle(m_hInitEvent);
    m_hInitEvent = NULL;
    if (m_iMainThreadInitResult != 200) {
        m_lasterr = L"主线程初始化失败";
        return FALSE;
    }
    return TRUE;
}

BOOL CSoTunnel::InitRemote()
{
    m_pChnSocket = new CChannelSocket(&m_asmgr);
    m_pChnSocket->SetCallback(this);
    m_pChnSocket->m_keepalive_timeout_sec = m_keepalive_timeout_second;
    m_pChnSocket->m_bRemoteSide = TRUE;
    m_pChnSocket->m_config.mode = stmode_socks5;
    m_pTunnelIO = Cnbsocket::createInstance();

    SOCKET tunnelSocketIO;
    if (!socket_mkpipes(&m_pChnSocket->m_hSocket, &tunnelSocketIO, true)) {
        m_lasterr = L"初始化管道失败";
        return FALSE;
    }
    socket_setbufsize(m_pChnSocket->m_hSocket, ChnSocketBufferSize);
    socket_setbufsize(tunnelSocketIO, ChnSocketBufferSize);
    m_pTunnelIO->Attach(tunnelSocketIO);
    m_hInitEvent = CreateEvent(0, 1, 0, 0);
    CThread::Start();
    WaitForSingleObject(m_hInitEvent, INFINITE);
    CloseHandle(m_hInitEvent);
    m_hInitEvent = NULL;
    if (m_iMainThreadInitResult != 200) {
        m_lasterr = L"主线程初始化失败";
        return FALSE;
    }
    return TRUE;
}

void CSoTunnel::Deinit()
{
    if (CThread::GetHandle()) {
        PostThreadMessage(CThread::GetThreadId(), WM_QUIT, 0, 0);
        CThread::Wait();
    }

    if (m_pChnSocket) {
        delete m_pChnSocket;
        m_pChnSocket = NULL;
    }
    if (m_pAcceptor) {
        delete m_pAcceptor;
        m_pAcceptor = NULL;
    }
    if (m_pTunnelIO) {
        m_pTunnelIO->Dereference();
        m_pTunnelIO = NULL;
    }
}

BOOL CSoTunnel::AddASocket(Casocket* as, long lEvent)
{
    return m_asmgr.AddSocket(as, lEvent);
}

BOOL CSoTunnel::SendConfig(unsigned char mode, const char* ip, unsigned short port)
{
    m_pChnSocket->ctrl_config(mode, ip, port);
    m_pChnSocket->Trigger(FD_WRITE, 0);
    return TRUE;
}

BOOL CSoTunnel::SendReset()
{
    m_pChnSocket->ctrl_reset();
    m_pChnSocket->Trigger(FD_WRITE, 0);
    return TRUE;
}

BOOL CSoTunnel::SendRUReady()
{
    m_pChnSocket->ctrl_askready();
    m_pChnSocket->Trigger(FD_WRITE, 0);
    return TRUE;
}

DWORD CSoTunnel::run()
{
    MSG msg;
    int bRet;

    bRet = m_asmgr.Init();
    if (!bRet)
        goto _ERROR;
    if (m_pAcceptor) {
        bRet = m_asmgr.AddSocket(m_pAcceptor, FD_ACCEPT);
        if (!bRet)
            goto _ERROR;
    }
    bRet = m_asmgr.AddSocket(m_pChnSocket, 0);
    if (!bRet)
        goto _ERROR;
    if (!m_pChnSocket->Init())
        goto _ERROR;

    m_iMainThreadInitResult = 200;
    SetEvent(m_hInitEvent);
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1) {
            break;
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
_ERROR:
    if (m_hInitEvent)
        SetEvent(m_hInitEvent);
    m_asmgr.Deinit();
    if (m_pChnSocket) {
        delete m_pChnSocket;
        m_pChnSocket = NULL;
    }
    if (m_pAcceptor) {
        delete m_pAcceptor;
        m_pAcceptor = NULL;
    }
    return 0;
}

void CSoTunnel::OnChannelSocketClose()
{
    PostThreadMessage(CThread::GetThreadId(), WM_QUIT, 0, 0);
}
