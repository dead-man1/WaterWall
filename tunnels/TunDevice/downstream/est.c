#include "structure.h"

#include "loggers/network_logger.h"

void tundeviceTunnelDownStreamEst(tunnel_t *t, line_t *l)
{
    // packet tunnels dont care about this callabck
    discard t;
    discard l;
}
