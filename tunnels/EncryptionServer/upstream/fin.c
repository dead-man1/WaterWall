#include "structure.h"

#include "loggers/network_logger.h"

void encryptionserverTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    encryptionserver_lstate_t *ls = lineGetState(l, t);

    if (! ls->next_finished)
    {
        ls->next_finished = true;
        tunnelNextUpStreamFinish(t, l);
    }

    encryptionserverLinestateDestroy(ls);
}
