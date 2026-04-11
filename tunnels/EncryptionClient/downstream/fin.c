#include "structure.h"

#include "loggers/network_logger.h"

void encryptionclientTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    encryptionclient_lstate_t *ls = lineGetState(l, t);

    if (! ls->prev_finished)
    {
        ls->prev_finished = true;
        tunnelPrevDownStreamFinish(t, l);
    }

    encryptionclientLinestateDestroy(ls);
}
