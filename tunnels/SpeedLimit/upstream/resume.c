#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    if (ls->next_side_external_pause_depth > 0)
    {
        ls->next_side_external_pause_depth -= 1;
    }

    if (! ls->next_side_locally_paused && ls->next_side_external_pause_depth == 0)
    {
        tunnelNextUpStreamResume(t, l);
    }
}
