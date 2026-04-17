#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelUpStreamPause(tunnel_t *t, line_t *l)
{
    speedlimit_lstate_t *ls = lineGetState(l, t);
    if (ls->next_side_external_pause_depth < UINT16_MAX)
    {
        ls->next_side_external_pause_depth += 1;
    }

    tunnelNextUpStreamPause(t, l);
}
