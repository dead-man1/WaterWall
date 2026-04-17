#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDestroy(tunnel_t *t)
{
    tunnelDestroy(t);
}
