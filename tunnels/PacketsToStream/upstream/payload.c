#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    packetstostream_lstate_t *ls = lineGetState(l, t);
    line_t                   *stream_line;

    stream_line = packetstostreamEnsureOutputLine(t, l, ls);

    if (stream_line == NULL || ls->paused)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    tunnelNextUpStreamPayload(t, stream_line, buf);
}
