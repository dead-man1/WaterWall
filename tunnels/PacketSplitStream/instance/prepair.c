#include "structure.h"

#include "loggers/network_logger.h"

void packetsplitstreamTunnelOnPrepair(tunnel_t *t)
{
    // the 'up' and 'down' tunnels are already added to chain and OnPrepair call back gets called automatically on them.
    
    discard t;
}

