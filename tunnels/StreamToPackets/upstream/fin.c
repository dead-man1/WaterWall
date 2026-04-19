#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    line_t                   *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    streamtopackets_lstate_t *packet_ls   = lineGetState(packet_line, t);
    streamtopackets_lstate_t *line_ls     = lineGetState(l, t);

    if (packet_ls->line == l)
    {
        packet_ls->line   = NULL;
        packet_ls->paused = false;
    }

    if (line_ls->read_stream.pool != NULL)
    {
        streamtopacketsLinestateDestroy(line_ls);
    }
}
