#include "structure.h"

#include "loggers/network_logger.h"

void disturberTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    disturber_tstate_t *ts = tunnelGetState(t);
    disturber_lstate_t *ls = lineGetState(l, t);

    disturberLinestateInitialize(ls);

    if (roll100(ts->chance_instant_close))
    {
        LOGD("Disturber: Closing connection instantly");
        tunnelPrevDownStreamFinish(t, l);
        return;
    }

    tunnelNextUpStreamInit(t, l);
}
