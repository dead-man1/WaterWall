#include "structure.h"

#include "loggers/network_logger.h"

void httpserverTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    httpserver_lstate_t *ls = lineGetState(l, t);

    if (ls->runtime_proto == kHttpServerRuntimeHttp2)
    {
        if (! httpserverTransportFeedHttp2Input(t, l, ls, buf))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
            return;
        }

        if (! httpserverTransportFlushPendingDown(t, l, ls))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
            return;
        }

        return;
    }

    bufferstreamPush(&ls->in_stream, buf);

    if (! httpserverTransportDetectRuntimeProtocol(t, l, ls))
    {
        httpserverTransportCloseBothDirections(t, l, ls);
        return;
    }

    if (ls->runtime_proto == kHttpServerRuntimeHttp2)
    {
        while (! bufferstreamIsEmpty(&ls->in_stream))
        {
            sbuf_t *frame = bufferstreamIdealRead(&ls->in_stream);
            if (! httpserverTransportFeedHttp2Input(t, l, ls, frame))
            {
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
        }

        if (! httpserverTransportFlushPendingDown(t, l, ls))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
        }

        return;
    }

    if (! httpserverTransportHandleHttp1RequestHeaderPhase(t, l, ls))
    {
        httpserverTransportCloseBothDirections(t, l, ls);
        return;
    }

    if (ls->runtime_proto == kHttpServerRuntimeHttp2)
    {
        while (! bufferstreamIsEmpty(&ls->in_stream))
        {
            sbuf_t *frame = bufferstreamIdealRead(&ls->in_stream);
            if (! httpserverTransportFeedHttp2Input(t, l, ls, frame))
            {
                httpserverTransportCloseBothDirections(t, l, ls);
                return;
            }
        }

        if (! httpserverTransportFlushPendingDown(t, l, ls))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
        }

        return;
    }

    if (ls->runtime_proto == kHttpServerRuntimeHttp1 && ls->h1_headers_parsed)
    {
        if (! httpserverTransportDrainHttp1RequestBody(t, l, ls))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
            return;
        }
    }

    if (! httpserverTransportFlushPendingDown(t, l, ls))
    {
        httpserverTransportCloseBothDirections(t, l, ls);
    }
}
