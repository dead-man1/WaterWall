#include "structure.h"

#include "loggers/network_logger.h"

void httpserverTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    httpserver_lstate_t *ls = lineGetState(l, t);

    if (ls->runtime_proto == kHttpServerRuntimeHttp1)
    {
        if (! ls->h1_headers_parsed)
        {
            bufferqueuePushBack(&ls->pending_down, buf);
            return;
        }

        if (! ls->h1_response_headers_sent)
        {
            if (! httpserverTransportSendHttp1ResponseHeaders(t, l))
            {
                lineReuseBuffer(l, buf);
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
            ls->h1_response_headers_sent = true;
        }

        if (! httpserverTransportSendHttp1ChunkedPayload(t, l, buf))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
        }
        return;
    }

    if (ls->runtime_proto == kHttpServerRuntimeHttp2)
    {
        if (ls->h2_stream_id <= 0)
        {
            bufferqueuePushBack(&ls->pending_down, buf);
            return;
        }

        if (! ls->h2_response_headers_sent)
        {
            if (! httpserverTransportSubmitHttp2ResponseHeaders(t, l, ls, false))
            {
                lineReuseBuffer(l, buf);
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
        }

        if (! httpserverTransportSendHttp2DataFrame(t, l, ls, buf, false))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
        }
        return;
    }

    bufferqueuePushBack(&ls->pending_down, buf);
}
