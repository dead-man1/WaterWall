#include "structure.h"

#include "loggers/network_logger.h"

void encryptionserverTunnelDestroy(tunnel_t *t)
{
    encryptionserver_tstate_t *ts = tunnelGetState(t);
    encryptionserverTunnelstateDestroy(ts);
    tunnelDestroy(t);
}
