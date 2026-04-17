#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamPause(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    if (ls->prev_side_external_pause_depth < UINT16_MAX)
    {
        ls->prev_side_external_pause_depth += 1;
    }

    tunnelPrevDownStreamPause(t, l);
}
