#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamPause(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    ls->prev_side_externally_paused = true;
    tunnelPrevDownStreamPause(t, l);
}
