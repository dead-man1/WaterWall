#include "structure.h"

#include "loggers/network_logger.h"

void reverseserverTunnelUpStreamResume(tunnel_t *t, line_t *d)
{
    reverseserver_lstate_t *dls = lineGetState(d, t);

    if (! dls->paired || dls->u == NULL)
    {
        return;
    }

    tunnelNextUpStreamResume(t, dls->u);
}
