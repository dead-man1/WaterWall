#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDownStreamResume(tunnel_t *t, line_t *l)
{
    packetstostream_lstate_t *ls = lineGetState(tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l)), t);

    ls->paused = false;
}
