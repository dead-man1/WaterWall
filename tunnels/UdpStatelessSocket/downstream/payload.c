#include "structure.h"

#include "loggers/network_logger.h"

void udpstatelesssocketTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    // sharing the exact code with upstream side (bidirectional tunnel)
    udpstatelesssocket_tstate_t *state = tunnelGetState(t);
    if (lineGetWID(l) != state->io_wid)
    {
        lineScheduleTaskWithBuf(l, udpstatelesssocketLocalThreadSocketUpStream, t, buf);
    }
    else
    {
        udpstatelesssocketLocalThreadSocketUpStream(t, l, buf);
    }
}
