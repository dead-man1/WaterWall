#include "structure.h"

#include "loggers/network_logger.h"

void testerserverTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    discard t;
    discard l;
    LOGF("TesterServer: downStreamFinish disabled");
    assert(false);
}
