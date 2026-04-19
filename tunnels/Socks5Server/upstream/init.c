#include "structure.h"

#include "loggers/network_logger.h"

void socks5serverTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    if (lineGetSourceAddressContext(l)->proto_tcp)
    {
        socks5serverLinestateInitialize(lineGetState(l, t), t, l, kSocks5ServerLineKindControlTcp);
        return;
    }

    if (lineGetSourceAddressContext(l)->proto_udp)
    {
        if (l == tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l)))
        {
            socks5serverLinestateInitialize(lineGetState(l, t), t, l, kSocks5ServerLineKindNone);
            return;
        }

        socks5serverLinestateInitialize(lineGetState(l, t), t, l, kSocks5ServerLineKindUdpClient);
        return;
    }

    socks5serverLinestateInitialize(lineGetState(l, t), t, l, kSocks5ServerLineKindNone);
    if (! withLineLocked(l, tunnelNextUpStreamInit, t))
    {
        return;
    }
}
