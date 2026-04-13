#include "structure.h"

#include "loggers/network_logger.h"

void pingclientDestroy(tunnel_t *t)
{
    tunnelDestroy(t);
}
