#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    ls->next_side_externally_paused = false;
    tunnelNextUpStreamResume(t, l);

    if (ls->next_side_locally_paused)
    {
        tunnelNextUpStreamPause(t, l);
    }
}
