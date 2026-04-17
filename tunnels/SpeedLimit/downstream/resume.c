#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamResume(tunnel_t *t, line_t *l)
{
    speedlimitHandleDownstreamResume(t, l);
}
