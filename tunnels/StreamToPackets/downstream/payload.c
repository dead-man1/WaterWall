#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    line_t                 *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    streamtopackets_lstate_t *ls        = lineGetState(packet_line, t);

    if (ls->paused || ls->line == NULL || ! lineIsAlive(ls->line))
    {
        lineReuseBuffer(l, buf);
        return;
    }

    tunnelPrevDownStreamPayload(t, ls->line, buf);
}
