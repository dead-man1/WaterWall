#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    line_t                   *packet_line = tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l));
    packetstostream_lstate_t *ls          = lineGetState(packet_line, t);

    if (ls->line == l)
    {
        LOGD("PacketsToStream: got fin, recreating line");
        ls->line   = NULL;
        ls->paused = false;

        if (ls->read_stream.pool != NULL)
        {
            bufferstreamEmpty(&ls->read_stream);
        }

        if (lineIsAlive(l))
        {
            lineDestroy(l);
        }

        if (! ls->recreate_scheduled)
        {
            ls->recreate_scheduled = true;
            lineScheduleTask(packet_line, packetstostreamRecreateOutputLineTask, t);
        }

        return;
    }

    if (lineIsAlive(l))
    {
        lineDestroy(l);
    }
}
