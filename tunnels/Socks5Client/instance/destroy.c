#include "structure.h"

#include "loggers/network_logger.h"

void socks5clientTunnelDestroy(tunnel_t *t)
{
    if (t == NULL)
    {
        return;
    }

    socks5clientTunnelstateDestroy(tunnelGetState(t));
    tunnelDestroy(t);
}
