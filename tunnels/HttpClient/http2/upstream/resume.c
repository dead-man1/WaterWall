#include "structure.h"

#include "loggers/network_logger.h"

void httpclientTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    tunnelNextUpStreamResume(t, l);
}
