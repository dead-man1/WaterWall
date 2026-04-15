#include "structure.h"

#include "loggers/network_logger.h"

void testerserverTunnelDownStreamInit(tunnel_t *t, line_t *l)
{
    discard t;
    discard l;
    LOGF("TesterServer: downStreamInit disabled");
    assert(false);
}
