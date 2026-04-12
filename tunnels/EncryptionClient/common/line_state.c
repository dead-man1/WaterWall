#include "structure.h"

#include "loggers/network_logger.h"

void encryptionclientLinestateInitialize(encryptionclient_lstate_t *ls, buffer_pool_t *pool)
{

    ls->read_stream   = bufferstreamCreate(pool, kEncryptionFramePrefixSize);
    ls->next_finished = false;
    ls->prev_finished = false;
}

void encryptionclientLinestateDestroy(encryptionclient_lstate_t *ls)
{

    bufferstreamDestroy(&ls->read_stream);

    memoryZeroAligned32(ls, sizeof(encryptionclient_lstate_t));
}
