#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    line_t                 *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    streamtopackets_lstate_t *ls        = lineGetState(packet_line, t);

    if (ls->line == l)
    {
        ls->paused = false;
    }
}
