#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelDownStreamEst(tunnel_t *t, line_t *l)
{
    tlsserver_lstate_t *ls = lineGetState(l, t);

    if (ls->handshake_completed)
    {
        if (! lineIsEstablished(l))
        {
            tunnelPrevDownStreamEst(t, l);
        }
        return;
    }

    ls->downstream_est_pending = true;
}
