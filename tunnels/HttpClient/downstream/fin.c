#include "structure.h"

#include "loggers/network_logger.h"

void httpclientTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    httpclient_lstate_t *ls = lineGetState(l, t);
    bool                 already_finished = false;

    if (ls->initialized)
    {
        already_finished = ls->prev_finished;
        httpclientLinestateDestroy(ls);
    }

    if (! already_finished)
    {
        tunnelPrevDownStreamFinish(t, l);
    }
}
