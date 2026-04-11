#include "structure.h"

#include "loggers/network_logger.h"

void encryptionserverTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    encryptionserver_lstate_t *ls = lineGetState(l, t);

    if (! ls->prev_finished)
    {
        ls->prev_finished = true;
        tunnelPrevDownStreamFinish(t, l);
    }

    encryptionserverLinestateDestroy(ls);
}
