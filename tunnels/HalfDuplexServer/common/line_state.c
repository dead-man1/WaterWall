#include "structure.h"

#include "loggers/network_logger.h"

void halfduplexserverLinestateInitialize(halfduplexserver_lstate_t *ls)
{
    *ls = (halfduplexserver_lstate_t){.state = kCsUnkown,.upload_line = NULL,.download_line = NULL,.main_line = NULL};
}

void halfduplexserverLinestateDestroy(halfduplexserver_lstate_t *ls)
{
    assert(ls->buffering == NULL);
    memoryZeroAligned32(ls, sizeof(halfduplexserver_lstate_t));
}
