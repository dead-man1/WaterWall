#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    line_t                 *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    streamtopackets_lstate_t *ls        = lineGetState(packet_line, t);

    if (ls->read_stream.pool == NULL)
    {
        streamtopacketsLinestateInitialize(ls, lineGetBufferPool(l));
    }

    if (ls->line != l)
    {
        if (ls->line != NULL)
        {
            LOGW("StreamToPackets: replacing active upstream line on worker %u", (unsigned int) lineGetWID(l));
        }

        // Packet parsing state is worker-local, so partial bytes must not survive a line switch.
        streamtopacketsLinestateReset(ls);
    }

    ls->paused = false;
    ls->line   = l;
}
