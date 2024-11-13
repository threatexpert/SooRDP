// 测试SoTunnel
//

#include "pch.h"
#include "../misc/netutils.h"
#include "../misc/SoTunnel.h"
#include <string>

CASocketMgr gasmgr;

void remotemode(int listen_port)
{
    printf("remotemode\n");
    class CSoTunnelR : public CSoTunnel
    {
    public:
        virtual void OnChannelConfigChanged(st_config* config) {
            if (config->mode == stmode_socks5)
                printf("remote config changed: mode=socks5, ip=%s, port=%d\n", config->ip, (int)config->port);
            else
                printf("remote config changed: mode=redir, ip=%s, port=%d\n", config->ip, (int)config->port);
        };
        virtual void OnChannelSocketClose()
        {
            printf("remote: OnChannelSocketClose\n");
            CSoTunnel::OnChannelSocketClose();
        }

    }sot;

    do
    {
        printf("listening port %d...\n", listen_port);
        SOCKET lis = socket_createListener(0, listen_port, 1, 0, 1);
        if (lis == INVALID_SOCKET) {
            printf("err: init socket_createListener\n");
            break;
        }
        SOCKET sClient = INVALID_SOCKET;
        socket_accept(lis, 15, &sClient, true);
        if (sClient == INVALID_SOCKET) {
            printf("err: initsocket_accept\n");
            break;
        }
        closesocket(lis);
        printf("a client accepted\n");
        socket_setbufsize(sClient, ChnSocketBufferSize);
        if (!sot.InitRemote()) {
            printf("err: init remote\n");
            break;
        }

        CasTcpPair sa, sb;
        sa.m_hSocket = sot.m_pTunnelIO->Detach();
        sa.m_pp = &sb;
        sb.m_hSocket = sClient;
        sb.m_pp = &sa;

        sot.AddASocket(&sa, FD_READ | FD_WRITE | FD_CLOSE);
        sot.AddASocket(&sb, FD_READ | FD_WRITE | FD_CLOSE);

        sot.Wait();
    } while (0);

    printf("exit\n");
    exit(1);
}


void localmode(int local_port, const char *server, int remote_port)
{
    printf("localmode\n");
    class CSoTunnelL : public CSoTunnel
    {
    public:
        virtual void OnChannelSocketReady(bool ok) {
            printf("local: OnChannelSocketReady\n");
            SendConfig(stmode_socks5, "", 0);
        };
        virtual void OnChannelSocketClose()
        {
            printf("remote: OnChannelSocketClose\n");
            CSoTunnel::OnChannelSocketClose();
        }

    }sot;

    do {
        printf("connecting %s:%d...\n", server, remote_port);
        SOCKET sRemote = socket_connect(server, remote_port, 10000, true);
        if (sRemote == INVALID_SOCKET) {
            printf("err: socket_connect\n");
            break;
        }
        printf("connected.\n");
        socket_setbufsize(sRemote, ChnSocketBufferSize);
        if (!sot.InitLocal(local_port)) {
            printf("err: init local\n");
            break;
        }
        printf("local listening port %d...\n", local_port);
        CasTcpPair sa, sb;
        sa.m_hSocket = sot.m_pTunnelIO->Detach();
        sa.m_pp = &sb;
        sb.m_hSocket = sRemote;
        sb.m_pp = &sa;

        sot.AddASocket(&sa, FD_READ | FD_WRITE | FD_CLOSE);
        sot.AddASocket(&sb, FD_READ | FD_WRITE | FD_CLOSE);
        
        sot.Wait();
    } while (0);

    printf("exit\n");
    exit(1);
}

int main(int argc, char **argv)
{
    socket_init_ws32();
	const char* mode = "r";
    int local_port = 8888;
    int remote_port = 8888;
    const char* serverip = "127.0.0.1";
    int i;
	if (argc <= 1) {

        printf("Only For Testing SoTunnel.\n");
        printf("usage:\n");
        printf("local mode:\n");
        printf("    tt -l 8888 -r x.x.x.x -p 8888\n");
        printf("remote mode:\n");
        printf("    tt -p 8888\n");
        return 0;
    }

    for (i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "-l") == 0 && i + 1 < argc)
        {
            mode = "l";
            local_port = atoi(argv[i + 1]);
            i++;
        }
        else if (_stricmp(argv[i], "-r") == 0 && i + 1 < argc)
        {
            serverip = argv[i + 1];
            i++;
        }
        else if (_stricmp(argv[i], "-p") == 0 && i + 1 < argc)
        {
            remote_port = atoi(argv[i + 1]);
            i++;
        }
        else {
            break;
        }
    }

    socket_init_ws32();

	if (strcmp(mode, "r") == 0) {
		remotemode(remote_port);
	}
	else if (strcmp(mode, "l") == 0) {
		localmode(local_port, serverip, remote_port);
	}
}

