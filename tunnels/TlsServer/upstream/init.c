#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    tlsserver_tstate_t *ts = tunnelGetState(t);
    tlsserver_lstate_t *ls = lineGetState(l, t);

    if (! tlsserverLinestateInitialize(ls, ts->threadlocal_ssl_contexts[lineGetWID(l)]))
    {
        LOGE("TlsServer: failed to initialize per-line OpenSSL state");
        tunnelPrevDownStreamFinish(t, l);
        return;
    }

    if (! withLineLocked(l, tunnelNextUpStreamInit, t))
    {
        return;
    }
}
