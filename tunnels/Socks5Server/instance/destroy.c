#include "structure.h"

#include "loggers/network_logger.h"

void socks5serverTunnelDestroy(tunnel_t *t)
{
    if (t == NULL)
    {
        return;
    }

    socks5serverTunnelstateDestroy(tunnelGetState(t));
    tunnelDestroy(t);
}
