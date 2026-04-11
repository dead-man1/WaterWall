#include "structure.h"

#include "loggers/network_logger.h"

void httpserverTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    httpserver_tstate_t *ts = tunnelGetState(t);
    httpserver_lstate_t *ls = lineGetState(l, t);

    httpserverLinestateInitialize(ls, t, l);

    if (! withLineLocked(l, tunnelNextUpStreamInit, t))
    {
        return;
    }

    if (ts->version_mode == kHttpServerVersionModeHttp1)
    {
        ls->runtime_proto = kHttpServerRuntimeHttp1;
        return;
    }

    if (ts->version_mode == kHttpServerVersionModeHttp2)
    {
        if (! httpserverTransportEnsureHttp2Session(t, l, ls))
        {
            httpserverTransportCloseBothDirections(t, l, ls);
        }
        return;
    }

    ls->runtime_proto = kHttpServerRuntimeUnknown;
}
