#include "structure.h"

#include "loggers/network_logger.h"

void pingserverLinestateInitialize(pingserver_lstate_t *ls)
{
    discard ls;
}

void pingserverLinestateDestroy(pingserver_lstate_t *ls)
{
    memorySet(ls, 0, sizeof(pingserver_lstate_t));
}
