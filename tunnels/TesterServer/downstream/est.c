#include "structure.h"

#include "loggers/network_logger.h"

void testerserverTunnelDownStreamEst(tunnel_t *t, line_t *l)
{
    discard t;
    discard l;
    LOGF("TesterServer: downStreamEst disabled");
    assert(false);
}
