#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamPause(tunnel_t *t, line_t *l)
{
    speedlimitHandleDownstreamPause(t, l);
}
