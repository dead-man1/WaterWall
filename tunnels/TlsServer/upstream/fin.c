#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    tlsserver_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    if (! ls->next_finished)
    {
        ls->next_finished = true;
        tlsserverLinestateDestroy(ls);
        tunnelNextUpStreamFinish(t, l);
    }
    else
    {
        tlsserverLinestateDestroy(ls);
    }

    lineUnlock(l);
}
