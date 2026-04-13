#include "structure.h"

#include "loggers/network_logger.h"

void pingserverDestroy(tunnel_t *t)
{
    tunnelDestroy(t);
}
