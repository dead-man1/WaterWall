#include "structure.h"

#include "loggers/network_logger.h"

static void failAndCloseU(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    httpclientLinestateDestroy(ls);
    tunnelNextUpStreamFinish(t, l);
    tunnelPrevDownStreamFinish(t, l);
}

void httpclientTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    httpclient_lstate_t *ls = lineGetState(l, t);

    if (ls->runtime_proto == kHttpClientRuntimeHttp1)
    {
        if (! httpclientTransportSendHttp1ChunkedPayload(t, l, buf))
        {
            failAndCloseU(t, l, ls);
        }
        return;
    }

    if (ls->runtime_proto == kHttpClientRuntimeHttp2)
    {
        if (! httpclientTransportSendHttp2DataFrame(t, l, ls, buf, false))
        {
            failAndCloseU(t, l, ls);
        }
        return;
    }

    bufferqueuePushBack(&ls->pending_up, buf);
}
