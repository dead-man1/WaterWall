#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamResume(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    if (ls->prev_side_external_pause_depth > 0)
    {
        ls->prev_side_external_pause_depth -= 1;
    }

    if (! ls->prev_side_locally_paused && ls->prev_side_external_pause_depth == 0)
    {
        tunnelPrevDownStreamResume(t, l);
    }
}
