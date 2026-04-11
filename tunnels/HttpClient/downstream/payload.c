#include "structure.h"

#include "loggers/network_logger.h"

static void failAndClose(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    httpclientLinestateDestroy(ls);
    tunnelNextUpStreamFinish(t, l);
    tunnelPrevDownStreamFinish(t, l);
}

void httpclientTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    httpclient_lstate_t *ls = lineGetState(l, t);

    if (ls->runtime_proto == kHttpClientRuntimeHttp2)
    {
        if (! httpclientTransportFeedHttp2Input(t, l, ls, buf))
        {
            failAndClose(t, l, ls);
        }
        return;
    }

    bufferstreamPush(&ls->in_stream, buf);

    if (! httpclientTransportHandleHttp1ResponseHeaderPhase(t, l, ls))
    {
        failAndClose(t, l, ls);
        return;
    }

    if (ls->runtime_proto == kHttpClientRuntimeHttp2)
    {
        while (! bufferstreamIsEmpty(&ls->in_stream))
        {
            sbuf_t *leftover = bufferstreamIdealRead(&ls->in_stream);
            if (! httpclientTransportFeedHttp2Input(t, l, ls, leftover))
            {
                failAndClose(t, l, ls);
                return;
            }
        }

        return;
    }

    if (ls->runtime_proto == kHttpClientRuntimeHttp1 && ls->h1_headers_parsed)
    {
        if (! httpclientTransportDrainHttp1Body(t, l, ls))
        {
            failAndClose(t, l, ls);
            return;
        }
    }
}
