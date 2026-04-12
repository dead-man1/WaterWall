#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    packetstostream_lstate_t *ls = lineGetState(l, t);
    packetstostreamEnsureOutputLine(t, l, ls);
}
