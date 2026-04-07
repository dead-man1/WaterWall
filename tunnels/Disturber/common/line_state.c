#include "structure.h"

#include "loggers/network_logger.h"

void disturberLinestateInitialize(disturber_lstate_t *ls)
{
    *ls = (disturber_lstate_t){
        .is_deadhang = false,
        .held_payload = NULL
    };
}

void disturberLinestateDestroy(disturber_lstate_t *ls)
{
    if (ls->held_payload != NULL)
    {
        reuseBuffer(ls->held_payload);
        ls->held_payload = NULL;
    }
    memoryZeroAligned32(ls, sizeof(disturber_lstate_t));
}
