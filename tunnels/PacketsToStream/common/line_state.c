#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamLinestateInitialize(packetstostream_lstate_t *ls)
{
    discard ls;
}

void packetstostreamLinestateDestroy(packetstostream_lstate_t *ls)
{
    memoryZeroAligned32(ls, sizeof(packetstostream_lstate_t));
}
