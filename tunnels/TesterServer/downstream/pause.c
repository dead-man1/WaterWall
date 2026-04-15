#include "structure.h"

#include "loggers/network_logger.h"

void testerserverTunnelDownStreamPause(tunnel_t *t, line_t *l)
{
    discard t;
    discard l;
    LOGF("TesterServer: downStreamPause disabled");
    assert(false);
}
