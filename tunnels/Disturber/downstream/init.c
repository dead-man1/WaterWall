#include "structure.h"

#include "loggers/network_logger.h"

void disturberTunnelDownStreamInit(tunnel_t *t, line_t *l)
{
    disturber_lstate_t *ls = lineGetState(l, t);
    disturberLinestateInitialize(ls);

    tunnelPrevDownStreamInit(t, l);
}
