#include "structure.h"

#include "loggers/network_logger.h"

void testerclientTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    testerclient_tstate_t *ts = tunnelGetState(t);
    testerclient_lstate_t *ls = lineGetState(l, t);

    if (ts->packet_mode)
    {
        testerclientFail(t, l, "packet-mode received unexpected finish on worker packet line");
        return;
    }

    if (! ls->response_complete)
    {
        testerclientFail(t, l, "received finish before full response verification");
        return;
    }

    testerclientLinestateDestroy(ls);
    testerclientMarkWorkerComplete(t, l);

    if (lineIsAlive(l))
    {
        lineDestroy(l);
    }
}
