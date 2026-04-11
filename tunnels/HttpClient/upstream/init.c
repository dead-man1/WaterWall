#include "structure.h"

#include "loggers/network_logger.h"

void httpclientTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    httpclient_tstate_t *ts = tunnelGetState(t);
    httpclient_lstate_t *ls = lineGetState(l, t);

    httpclientLinestateInitialize(ls, t, l);

    if (! withLineLocked(l, tunnelNextUpStreamInit, t))
    {
        return;
    }

    if (ts->version_mode == kHttpClientVersionModeHttp1)
    {
        ls->runtime_proto = kHttpClientRuntimeHttp1;
        if (! httpclientTransportSendHttp1RequestHeaders(t, l, false))
        {
            httpclientLinestateDestroy(ls);
        }
        return;
    }

    if (ts->version_mode == kHttpClientVersionModeHttp2)
    {
        ls->runtime_proto = kHttpClientRuntimeHttp2;
        if (! httpclientTransportEnsureHttp2Session(t, l, ls))
        {
            httpclientLinestateDestroy(ls);
        }
        return;
    }

    if (ts->enable_upgrade)
    {
        ls->runtime_proto = kHttpClientRuntimeWaitUpgrade;
        if (! httpclientTransportSendHttp1RequestHeaders(t, l, true))
        {
            httpclientLinestateDestroy(ls);
        }
        return;
    }

    ls->runtime_proto = kHttpClientRuntimeHttp2;
    if (! httpclientTransportEnsureHttp2Session(t, l, ls))
    {
        httpclientLinestateDestroy(ls);
    }
}
