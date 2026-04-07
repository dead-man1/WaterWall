#include "structure.h"

#include "loggers/network_logger.h"

void disturberTunnelDestroy(tunnel_t *t)
{
    tunnelDestroy(t);
}

