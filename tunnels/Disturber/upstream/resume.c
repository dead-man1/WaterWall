#include "structure.h"

#include "loggers/network_logger.h"

void disturberTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    tunnelNextUpStreamResume(t, l);
}
