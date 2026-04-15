#include "structure.h"

#include "loggers/network_logger.h"

void testerserverTunnelDownStreamResume(tunnel_t *t, line_t *l)
{
    discard t;
    discard l;
    LOGF("TesterServer: downStreamResume disabled");
    assert(false);
}
