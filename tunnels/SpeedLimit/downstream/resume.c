#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamResume(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    ls->prev_side_externally_paused = false;
    tunnelPrevDownStreamResume(t, l);

    if (ls->prev_side_locally_paused)
    {
        tunnelPrevDownStreamPause(t, l);
    }
}
