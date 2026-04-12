#include "structure.h"

#include "loggers/network_logger.h"

void streamtopacketsLinestateInitialize(streamtopackets_lstate_t *ls, buffer_pool_t *pool)
{
    *ls = (streamtopackets_lstate_t) {.line        = NULL,
                                      .read_stream = bufferstreamCreate(pool, 0),
                                      .paused      = false};
}

void streamtopacketsLinestateDestroy(streamtopackets_lstate_t *ls)
{
    if (ls->read_stream.pool != NULL)
    {
        bufferstreamDestroy(&ls->read_stream);
    }

    memoryZeroAligned32(ls, sizeof(streamtopackets_lstate_t));
}
