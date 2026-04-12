#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    line_t                 *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    packetstostream_lstate_t *ls        = lineGetState(packet_line, t);

    if (ls->line != l || ls->read_stream.pool == NULL)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    bufferstreamPush(&(ls->read_stream), buf);

    if (packetstostreamReadStreamIsOverflowed(&(ls->read_stream)))
    {
        bufferstreamEmpty(&(ls->read_stream));
        return;
    }

    while (true)
    {
        sbuf_t *packet_buffer = NULL;

        if (! packetstostreamTryReadIPv4Packet(&(ls->read_stream), &packet_buffer))
        {
            break;
        }

        tunnelPrevDownStreamPayload(t, packet_line, packet_buffer);
    }
}
