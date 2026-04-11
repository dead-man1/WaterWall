#include "structure.h"

#include "loggers/network_logger.h"

void encryptionclientTunnelDestroy(tunnel_t *t)
{
    encryptionclient_tstate_t *ts = tunnelGetState(t);
    encryptionclientTunnelstateDestroy(ts);
    tunnelDestroy(t);
}
