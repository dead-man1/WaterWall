#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    tlsserver_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    if (ls->prev_finished)
    {
        lineUnlock(l);
        return;
    }

    ls->prev_finished = true;

    if (! tlsserverSendCloseNotify(t, l, ls))
    {
        if (lineIsAlive(l))
        {
            lineUnlock(l);
            tlsserverPrintSSLState(ls->ssl);
            tlsserverCloseLineFatal(t, l, false);
            return;
        }

        lineUnlock(l);
        return;
    }

    tlsserverLinestateDestroy(ls);
    tunnelPrevDownStreamFinish(t, l);

    lineUnlock(l);
}
