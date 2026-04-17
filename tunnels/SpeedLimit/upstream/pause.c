#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelUpStreamPause(tunnel_t *t, line_t *l)
{
    speedlimitHandleUpstreamPause(t, l);
}
