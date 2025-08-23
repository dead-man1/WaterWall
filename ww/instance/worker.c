#include "worker.h"
#include "context.h"
#include "global_state.h"
#include "managers/signal_manager.h"
#include "pipe_tunnel.h"
#include "tunnel.h"
#include "wevent.h"
#include "wloop.h"
#include "wthread.h"

#include "loggers/internal_logger.h"

thread_local wid_t tl_wid;

void workerFinish(worker_t *worker)
{
    if (worker->tid == getTID())
    {
        if (worker->loop)
        {
            wloopDestroy(&worker->loop);
        }

        genericpoolDestroy(worker->wios_pool);
        genericpoolDestroy(worker->context_pool);
        genericpoolDestroy(worker->pipetunnel_msg_pool);
        bufferpoolDestroy(worker->buffer_pool);

        worker->loop                = NULL;
        worker->wios_pool           = NULL;
        worker->context_pool        = NULL;
        worker->pipetunnel_msg_pool = NULL;
        worker->buffer_pool         = NULL;
    }
    else
    {
        if (worker->loop)
        {
            worker->loop->flags = worker->loop->flags | WLOOP_FLAG_RUN_ONCE;
            atomicThreadFence(memory_order_release);
        }
        else
        {
            // lwip thread
            genericpoolDestroy(worker->wios_pool);
            genericpoolDestroy(worker->context_pool);
            genericpoolDestroy(worker->pipetunnel_msg_pool);
            bufferpoolDestroy(worker->buffer_pool);

            worker->loop                = NULL;
            worker->wios_pool           = NULL;
            worker->context_pool        = NULL;
            worker->pipetunnel_msg_pool = NULL;
            worker->buffer_pool         = NULL;
        }
    }
}

void workerExitJoin(worker_t *worker)
{

    workerFinish(worker);
    safeThreadJoin(worker->thread);
}

void workerInit(worker_t *worker, wid_t wid, bool eventloop)
{
    *worker = (worker_t){.wid = wid};

    worker->wios_pool =
        genericpoolCreateWithDefaultAllocatorAndCapacity(GSTATE.masterpool_wios, sizeof(wio_t), RAM_PROFILE);

    worker->context_pool = genericpoolCreateWithDefaultAllocatorAndCapacity(GSTATE.masterpool_context_pools,
                                                                            sizeof(context_t), RAM_PROFILE);

    worker->pipetunnel_msg_pool = genericpoolCreateWithDefaultAllocatorAndCapacity(
        GSTATE.masterpool_pipetunnel_msg_pools, (uint32_t) pipeTunnelGetMesageSize(), RAM_PROFILE);

    worker->buffer_pool = bufferpoolCreate(GSTATE.masterpool_buffer_pools_large, GSTATE.masterpool_buffer_pools_small,
                                           RAM_PROFILE, PROPER_LARGE_BUFFER_SIZE(RAM_PROFILE), SMALL_BUFFER_SIZE);

    if (eventloop)
    {
        // note that loop depeneds on worker->buffer_pool
        worker->loop = wloopCreate(WLOOP_FLAG_AUTO_FREE, worker->buffer_pool, wid);
    }
    else
    {
        worker->loop = NULL;
    }
}

void workerRun(worker_t *worker)
{
    tl_wid    = worker->wid;
    wid_t wid = worker->wid;
    frandInit();

    while (atomicLoadExplicit(&GSTATE.workers_run_flag, memory_order_acquire) == false)
    {
        if (atomicLoadExplicit(&GSTATE.application_stopping_flag, memory_order_acquire) == true)
        {
            LOGD("Worker %d exited", wid);
            return;
        }
        // wait for the main thread to set the flag
        wwSleepMS(10);
    }

    wloopRun(worker->loop);

    if (worker->wios_pool)
    {
        genericpoolDestroy(worker->wios_pool);
    }
    if (worker->context_pool)
    {
        genericpoolDestroy(worker->context_pool);
    }
    if (worker->pipetunnel_msg_pool)
    {
        genericpoolDestroy(worker->pipetunnel_msg_pool);
    }
    if (worker->buffer_pool)
    {
        bufferpoolDestroy(worker->buffer_pool);
    }

    worker->loop                = NULL;
    worker->wios_pool           = NULL;
    worker->context_pool        = NULL;
    worker->pipetunnel_msg_pool = NULL;
    worker->buffer_pool         = NULL;

    LOGD("Worker %d exited", wid);
}

/**
 * @brief Worker thread routine.
 *
 * This function is the entry point for the worker thread. It runs the worker and returns 0.
 *
 * @param userdata Pointer to the worker.
 * @return 0.
 */
static WTHREAD_ROUTINE(worker_thread) // NOLINT
{
    worker_t *worker = userdata;
    worker->tid      = getTID();
    workerRun(worker);

    return 0;
}

/**
 * @brief Runs the worker in a new thread.
 *
 * This function creates a new thread and runs the worker in it.
 *
 * @param worker Pointer to the worker to run.
 */
void workerSpawn(worker_t *worker)
{
    worker->thread = threadCreate(worker_thread, worker);
}
