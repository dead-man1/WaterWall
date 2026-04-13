#include "structure.h"

#include "loggers/network_logger.h"

void httpserverTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    httpserver_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    httpserverLinestateDestroy(ls);
    tunnelNextUpStreamFinish(t, l);

    lineUnlock(l);
}
