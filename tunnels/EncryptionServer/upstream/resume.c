#include "structure.h"

#include "loggers/network_logger.h"

void encryptionserverTunnelUpStreamResume(tunnel_t *t, line_t *l)
{
    tunnelNextUpStreamResume(t, l);
}
