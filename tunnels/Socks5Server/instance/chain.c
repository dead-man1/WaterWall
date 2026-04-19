#include "structure.h"

#include "loggers/network_logger.h"

void socks5serverTunnelOnChain(tunnel_t *t, tunnel_chain_t *chain)
{
    discard t;
    discard chain;
    LOGF("Socks5Server: onChain override is disabled, use the default tunnel chaining");
    terminateProgram(1);
}
