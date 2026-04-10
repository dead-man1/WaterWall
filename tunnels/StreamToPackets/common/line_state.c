#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsLinestateInitialize(streamtopackets_lstate_t *ls)
{
    discard ls;
}

void streamtopacketsLinestateDestroy(streamtopackets_lstate_t *ls)
{
    memorySet(ls, 0, sizeof(streamtopackets_lstate_t));
}
