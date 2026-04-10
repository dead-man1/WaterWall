#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsTunnelUpStreamResume(tunnel_t *t, line_t *l)
{

    streamtopackets_lstate_t *ls = lineGetState(tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l)), t);

    ls->paused = false;
}
