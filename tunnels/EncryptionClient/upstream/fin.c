#include "structure.h"

#include "loggers/network_logger.h"

void encryptionclientTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    encryptionclient_lstate_t *ls = lineGetState(l, t);

    if (! ls->next_finished)
    {
        ls->next_finished = true;
        tunnelNextUpStreamFinish(t, l);
    }

    encryptionclientLinestateDestroy(ls);
}
