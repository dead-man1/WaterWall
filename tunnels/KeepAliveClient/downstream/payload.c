#include "structure.h"

#include "loggers/network_logger.h"

void keepaliveclientTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    keepaliveclient_lstate_t *ls = lineGetState(l, t);

    bufferstreamPush(&ls->read_stream, buf);

    if (! keepaliveclientConsumeDownstreamFrames(t, l))
    {
        return;
    }
}
