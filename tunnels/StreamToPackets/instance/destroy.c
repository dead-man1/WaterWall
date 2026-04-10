#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelDestroy(tunnel_t *t)
{
    tunnelDestroy(t);
}

