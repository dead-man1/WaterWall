#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamInit(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    speedlimitLinestateInitialize(ls, t, l);

    tunnelPrevDownStreamInit(t, l);
}
