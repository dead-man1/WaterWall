#include "structure.h"

#include "loggers/network_logger.h"

void testerclientTunnelOnPrepair(tunnel_t *t)
{
    testerclient_tstate_t *ts = tunnelGetState(t);

    if (ts->packet_mode && tunnelGetChain(t)->packet_lines == NULL)
    {
        LOGF("TesterClient: packet-mode requires packet lines; add a packet-layer tunnel in the chain");
        terminateProgram(1);
    }
}
