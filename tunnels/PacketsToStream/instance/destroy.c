#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDestroy(tunnel_t *t)
{
    tunnelDestroy(t);
}

