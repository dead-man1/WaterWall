#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    line_t                   *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    streamtopackets_lstate_t *line_ls     = lineGetState(l, t);

    if (line_ls->read_stream.pool == NULL)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    bufferstreamPush(&(line_ls->read_stream), buf);

    if (streamtopacketsReadStreamIsOverflowed(&(line_ls->read_stream)))
    {
        bufferstreamEmpty(&(line_ls->read_stream));
        return;
    }

    lineLock(l);

    while (true)
    {
        sbuf_t *packet_buffer = NULL;

        if (! streamtopacketsTryReadIPv4Packet(&(line_ls->read_stream), &packet_buffer))
        {
            break;
        }

        tunnelNextUpStreamPayload(t, packet_line, packet_buffer);

        if (! lineIsAlive(l))
        {
            lineUnlock(l);
            return;
        }
    }

    lineUnlock(l);
}
