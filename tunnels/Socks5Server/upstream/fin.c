#include "structure.h"

#include "loggers/network_logger.h"

void socks5serverTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    socks5server_lstate_t *ls = lineGetState(l, t);

    switch (ls->kind)
    {
    case kSocks5ServerLineKindControlTcp:
        socks5serverCloseControlLineBidirectional(t, l);
        return;

    case kSocks5ServerLineKindUdpClient:
        socks5serverCloseUdpClientLine(t, l);
        return;

    case kSocks5ServerLineKindUdpRemote:
        socks5serverCloseUdpRemoteLine(t, l);
        return;

    case kSocks5ServerLineKindNone:
    default:
        socks5serverLinestateDestroy(ls);
        if (! withLineLocked(l, tunnelNextUpStreamFinish, t))
        {
            return;
        }
        return;
    }
}
