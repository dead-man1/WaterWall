#include "structure.h"

#include "loggers/network_logger.h"

void tcpoverudpserverTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    tcpoverudpserver_lstate_t *ls = lineGetState(l, t);
    if (UNLIKELY(ls->k_handle == NULL || ! ls->can_upstream))
    {
        return;
    }
    tcpoverudpserverLinestateDestroy(ls);

    tunnelNextUpStreamFinish(t, l);
}
