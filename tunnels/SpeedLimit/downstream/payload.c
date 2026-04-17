#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    speedlimitHandleDownstreamPayload(t, l, buf);
}
