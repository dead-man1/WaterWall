#include "structure.h"

#include "loggers/network_logger.h"

void keepaliveclientTunnelDestroy(tunnel_t *t)
{
    keepaliveclient_tstate_t *ts = tunnelGetState(t);
    keepaliveclient_lstate_t *ls = NULL;
    keepaliveclient_lstate_t *next = NULL;

    if (ts->worker_timers != NULL)
    {
        for (wid_t wi = 0; wi < getWorkersCount(); ++wi)
        {
            if (ts->worker_timers[wi] != NULL)
            {
                weventSetUserData(ts->worker_timers[wi], NULL);
                wtimerDelete(ts->worker_timers[wi]);
                ts->worker_timers[wi] = NULL;
            }
        }
    }

    mutexLock(&ts->lines_mutex);
    ls            = ts->lines_head;
    ts->lines_head = NULL;

    while (ls != NULL)
    {
        next = ls->tracked_next;
        if (ls->read_stream.pool != NULL)
        {
            keepaliveclientLinestateDestroy(ls);
        }
        ls = next;
    }
    mutexUnlock(&ts->lines_mutex);

    if (ts->worker_timers != NULL)
    {
        memoryFree(ts->worker_timers);
        ts->worker_timers = NULL;
    }

    mutexDestroy(&ts->lines_mutex);
    tunnelDestroy(t);
}
