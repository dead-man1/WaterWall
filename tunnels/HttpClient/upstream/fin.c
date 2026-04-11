#include "structure.h"

#include "loggers/network_logger.h"

void httpclientTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    httpclient_lstate_t *ls = lineGetState(l, t);

    if (! ls->initialized)
    {
        tunnelNextUpStreamFinish(t, l);
        return;
    }

    if (ls->runtime_proto == kHttpClientRuntimeHttp1)
    {
        if (! ls->fin_sent)
        {
            ls->fin_sent = true;
            httpclientTransportSendHttp1FinalChunk(t, l);
        }
    }
    else if (ls->runtime_proto == kHttpClientRuntimeHttp2)
    {
        if (! ls->fin_sent)
        {
            ls->fin_sent = true;
            httpclientTransportSendHttp2DataFrame(t, l, ls, NULL, true);
        }
    }
    else
    {
        ls->fin_sent = true;
    }

    httpclientLinestateDestroy(ls);
    tunnelNextUpStreamFinish(t, l);
}
