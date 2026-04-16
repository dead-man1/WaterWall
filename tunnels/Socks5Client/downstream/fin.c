#include "structure.h"

#include "loggers/network_logger.h"

void socks5clientTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    socks5client_lstate_t *ls = lineGetState(l, t);

    lineLock(l);
    ls->prev_finished = true;
    socks5clientLinestateDestroy(ls);
    tunnelPrevDownStreamFinish(t, l);
    lineUnlock(l);
}
