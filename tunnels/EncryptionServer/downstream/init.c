#include "structure.h"

#include "loggers/network_logger.h"

void encryptionserverTunnelDownStreamInit(tunnel_t *t, line_t *l)
{
    discard t;
    discard l;
    LOGF("EncryptionServer: DownStreamInit is disabled");

    terminateProgram(1);
}
