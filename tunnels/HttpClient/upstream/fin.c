#include "structure.h"

#include "loggers/network_logger.h"

void httpclientTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    httpclient_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    if (ls->runtime_proto == kHttpClientRuntimeHttp1)
    {
        if (! ls->fin_sent)
        {
            ls->fin_sent = true;
            if (! httpclientTransportSendHttp1FinalChunk(t, l))
            {
                if (lineIsAlive(l))
                {
                    httpclientTransportCloseBothDirections(t, l, ls);
                }
                else
                {
                    httpclientLinestateDestroy(ls);
                }
                lineUnlock(l);
                return;
            }
        }
    }
    else if (ls->runtime_proto == kHttpClientRuntimeHttp2)
    {
        if (! ls->fin_sent)
        {
            ls->fin_sent = true;
            if (! httpclientTransportSendHttp2DataFrame(t, l, ls, NULL, true))
            {
                if (lineIsAlive(l))
                {
                    httpclientTransportCloseBothDirections(t, l, ls);
                }
                else
                {
                    httpclientLinestateDestroy(ls);
                }
                lineUnlock(l);
                return;
            }
        }
    }
    else
    {
        ls->fin_sent = true;
    }

    httpclientLinestateDestroy(ls);
    tunnelNextUpStreamFinish(t, l);
    lineUnlock(l);
}
