#include "structure.h"

#include "loggers/network_logger.h"

void encryptionclientLinestateInitialize(encryptionclient_lstate_t *ls, buffer_pool_t *pool)
{
    if (ls->initialized)
    {
        return;
    }

    ls->read_stream   = bufferstreamCreate(pool, kEncryptionFramePrefixSize);
    ls->next_finished = false;
    ls->prev_finished = false;
    ls->initialized = true;
}

void encryptionclientLinestateDestroy(encryptionclient_lstate_t *ls)
{
    if (ls->initialized)
    {
        bufferstreamDestroy(&ls->read_stream);
        ls->initialized = false;
    }

    memoryZeroAligned32(ls, sizeof(encryptionclient_lstate_t));
}
