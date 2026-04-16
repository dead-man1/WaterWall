#include "structure.h"

#include "loggers/network_logger.h"

void socks5clientTunnelDownStreamPause(tunnel_t *t, line_t *l)
{
    tunnelPrevDownStreamPause(t, l);
}
