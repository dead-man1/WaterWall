#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelDestroy(tunnel_t *t)
{
    tlsserverTunnelstateDestroy(tunnelGetState(t));
    tunnelDestroy(t);
}
