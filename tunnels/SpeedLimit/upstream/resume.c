#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    speedlimitHandleUpstreamResume(t, l);
}
