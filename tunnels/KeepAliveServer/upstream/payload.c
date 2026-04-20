#include "structure.h"

#include "loggers/network_logger.h"

void keepaliveserverTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    keepaliveserver_lstate_t *ls = lineGetState(l, t);

    bufferstreamPush(&ls->read_stream, buf);

    if (! keepaliveserverConsumeUpstreamFrames(t, l))
    {
        return;
    }
}
