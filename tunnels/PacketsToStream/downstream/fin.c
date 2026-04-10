#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    packetstostream_lstate_t *ls = lineGetState(tunnelchainGetWorkerPacketLine(tunnelGetChain(t), lineGetWID(l)), t);

    assert(ls->line);
    assert(ls->line == l);

    LOGD("PacketsToStream: got fin, recreating line");

    line_t *nl = lineCreate(tunnelchainGetLinePools(tunnelGetChain(t)), lineGetWID(l));

    lineDestroy(ls->line);
    ls->line = NULL;

    ls->paused = false;
    ls->line   = nl;
    bufferstreamEmpty(&ls->read_stream);

    tunnelNextUpStreamInit(t, nl);
}
