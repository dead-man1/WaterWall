#include "structure.h"

#include "loggers/network_logger.h"

void socks5clientTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    socks5client_lstate_t *ls = lineGetState(l, t);

    lineLock(l);
    ls->next_finished = true;
    socks5clientLinestateDestroy(ls);
    tunnelNextUpStreamFinish(t, l);
    lineUnlock(l);
}
