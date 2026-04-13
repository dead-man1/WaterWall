#include "structure.h"

#include "loggers/network_logger.h"

void pingclientLinestateInitialize(pingclient_lstate_t *ls)
{
    discard ls;
}

void pingclientLinestateDestroy(pingclient_lstate_t *ls)
{
    memorySet(ls, 0, sizeof(pingclient_lstate_t));
}
