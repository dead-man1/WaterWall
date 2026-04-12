#include "structure.h"

#include "loggers/network_logger.h"

void encryptionserverLinestateInitialize(encryptionserver_lstate_t *ls, buffer_pool_t *pool)
{

    ls->read_stream   = bufferstreamCreate(pool, kEncryptionFramePrefixSize);
    ls->next_finished = false;
    ls->prev_finished = false;
}

void encryptionserverLinestateDestroy(encryptionserver_lstate_t *ls)
{

    bufferstreamDestroy(&ls->read_stream);

    memoryZeroAligned32(ls, sizeof(encryptionserver_lstate_t));
}
