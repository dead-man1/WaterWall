#include "structure.h"

#include "loggers/network_logger.h"

static void failAndCloseU(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    if (lineIsAlive(l))
    {
        httpclientTransportCloseBothDirections(t, l, ls);
    }
    else
    {
        httpclientLinestateDestroy(ls);
    }
}

void httpclientTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    httpclient_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    if (ls->runtime_proto == kHttpClientRuntimeHttp1)
    {
        if (! httpclientTransportSendHttp1ChunkedPayload(t, l, buf))
        {
            failAndCloseU(t, l, ls);
            lineUnlock(l);
            return;
        }
        lineUnlock(l);
        return;
    }

    if (ls->runtime_proto == kHttpClientRuntimeHttp2)
    {
        if (! httpclientTransportSendHttp2DataFrame(t, l, ls, buf, false))
        {
            failAndCloseU(t, l, ls);
            lineUnlock(l);
            return;
        }
        lineUnlock(l);
        return;
    }

    if (ls->runtime_proto == kHttpClientRuntimeWaitUpgrade)
    {
        LOGE("HttpClient: h2c upgrade mode does not support request body payloads");
        lineReuseBuffer(l, buf);
        failAndCloseU(t, l, ls);
        lineUnlock(l);
        return;
    }

    bufferqueuePushBack(&ls->pending_up, buf);
    lineUnlock(l);
}
