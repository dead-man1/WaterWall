#include "structure.h"

#include "loggers/network_logger.h"

void tlsserverTunnelDownStreamResume(tunnel_t *t, line_t *l)
{
    tunnelPrevDownStreamResume(t, l);
}
