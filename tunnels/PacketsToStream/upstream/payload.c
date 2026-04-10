#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    packetstostream_lstate_t *ls = (packetstostream_lstate_t *) lineGetState(l, t);

    if (ls->line == NULL)
    {
        LOGF("PacketsToStream: in upstream we are supposed to have line");
        terminateProgram(1);
    }
    if (ls->paused)
    {
        lineReuseBuffer(l, buf);
        return;
    }
    tunnelNextUpStreamPayload(t, ls->line, buf);
}
