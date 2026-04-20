#include "structure.h"

#include "loggers/network_logger.h"

void keepaliveclientTunnelDownStreamEst(tunnel_t *t, line_t *l)
{
    tunnelPrevDownStreamEst(t, l);
}
