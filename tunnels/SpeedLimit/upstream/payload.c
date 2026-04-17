#include "structure.h"

#include "loggers/network_logger.h"

void speedlimitTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    speedlimitHandleUpstreamPayload(t, l, buf);
}
