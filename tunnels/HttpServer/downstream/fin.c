#include "structure.h"

#include "loggers/network_logger.h"

void httpserverTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    httpserver_lstate_t *ls = lineGetState(l, t);

    if (! ls->initialized)
    {
        tunnelPrevDownStreamFinish(t, l);
        return;
    }

    ls->next_finished = true;

    if (! httpserverTransportFlushPendingDown(t, l, ls))
    {
        httpserverTransportCloseBothDirections(t, l, ls);
        return;
    }

    if (ls->runtime_proto == kHttpServerRuntimeHttp1)
    {
        if (! ls->h1_response_headers_sent)
        {
            if (! httpserverTransportSendHttp1ResponseHeaders(t, l))
            {
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
            ls->h1_response_headers_sent = true;
        }

        if (! ls->fin_sent)
        {
            ls->fin_sent = true;
            if (! httpserverTransportSendHttp1FinalChunk(t, l))
            {
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
        }
    }
    else if (ls->runtime_proto == kHttpServerRuntimeHttp2)
    {
        if (! ls->h2_response_headers_sent)
        {
            if (! httpserverTransportSubmitHttp2ResponseHeaders(t, l, ls, true))
            {
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
            ls->fin_sent = true;
        }
        else if (! ls->fin_sent)
        {
            ls->fin_sent = true;
            if (! httpserverTransportSendHttp2DataFrame(t, l, ls, NULL, true))
            {
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
        }
    }

    ls->prev_finished = true;
    httpserverLinestateDestroy(ls);
    tunnelPrevDownStreamFinish(t, l);
}
