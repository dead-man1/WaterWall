#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    tlsserver_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    if (! ls->handshake_completed)
    {
        bufferqueuePushBack(&ls->pending_down, buf);
        lineUnlock(l);
        return;
    }

    if (! tlsserverEncryptAndSendApplicationData(t, l, ls, buf))
    {
        if (lineIsAlive(l))
        {
            lineUnlock(l);
            tlsserverPrintSSLState(ls->ssl);
            tlsserverCloseLineFatal(t, l, false);
            return;
        }
        lineUnlock(l);
        return;
    }

    lineUnlock(l);
}
