#include "structure.h"

#include "loggers/network_logger.h"

static void closeOrDestroyLine(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
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

void httpclientTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    httpclient_tstate_t *ts = tunnelGetState(t);
    httpclient_lstate_t *ls = lineGetState(l, t);

    httpclientLinestateInitialize(ls, t, l);

    lineLock(l);

    if (! withLineLocked(l, tunnelNextUpStreamInit, t))
    {
        httpclientLinestateDestroy(ls);
        lineUnlock(l);
        return;
    }

    if (ts->version_mode == kHttpClientVersionModeHttp1)
    {
        ls->runtime_proto = kHttpClientRuntimeHttp1;
        if (! httpclientTransportSendHttp1RequestHeaders(t, l, false))
        {
            closeOrDestroyLine(t, l, ls);
        }
        lineUnlock(l);
        return;
    }

    if (ts->version_mode == kHttpClientVersionModeHttp2)
    {
        ls->runtime_proto = kHttpClientRuntimeHttp2;
        if (! httpclientTransportEnsureHttp2Session(t, l, ls))
        {
            closeOrDestroyLine(t, l, ls);
        }
        lineUnlock(l);
        return;
    }

    if (ts->enable_upgrade)
    {
        ls->runtime_proto = kHttpClientRuntimeWaitUpgrade;
        if (! httpclientTransportSendHttp1RequestHeaders(t, l, true))
        {
            closeOrDestroyLine(t, l, ls);
        }
        lineUnlock(l);
        return;
    }

    ls->runtime_proto = kHttpClientRuntimeHttp2;
    if (! httpclientTransportEnsureHttp2Session(t, l, ls))
    {
        closeOrDestroyLine(t, l, ls);
    }

    lineUnlock(l);
}
