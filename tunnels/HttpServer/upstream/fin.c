#include "structure.h"

#include "loggers/network_logger.h"

void httpserverTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    httpserver_lstate_t *ls = lineGetState(l, t);

    if (! ls->initialized)
    {
        tunnelNextUpStreamFinish(t, l);
        return;
    }

    if (! ls->next_finished)
    {
        ls->next_finished = true;
        tunnelNextUpStreamFinish(t, l);
    }

    httpserverLinestateDestroy(ls);
}
