#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    line_t                   *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    packetstostream_lstate_t *ls          = lineGetState(packet_line, t);

    if (ls->line == l)
    {
        LOGD("PacketsToStream: got fin, recreating line");
        lineDestroy(l);
        ls->line   = NULL;
        ls->paused = false;

        packetstostreamEnsureOutputLine(t, packet_line, ls);
        return;
    }

    if (lineIsAlive(l))
    {
        lineDestroy(l);
    }
}
