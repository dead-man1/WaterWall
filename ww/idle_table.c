#include "idle_table.h"
#include "basic_types.h"
#include "hdef.h"
#include "hloop.h"
#include "hmutex.h"
#include "ww.h"

enum
{
    kVecCap = 32
};

#define i_TYPE                    heapq_idles_t, struct idle_item_s *
#define i_cmp                     -c_default_cmp                                // NOLINT
#define idletable_less_func(x, y) ((*(x))->expire_at_ms < (*(y))->expire_at_ms) // NOLINT
#define i_less                    idletable_less_func                           // NOLINT
#include "stc/pque.h"

#define i_TYPE hmap_idles_t, uint64_t, struct idle_item_s *
#include "stc/hmap.h"

struct idle_table_s
{
    hloop_t       *loop;
    htimer_t      *idle_handle;
    heapq_idles_t  hqueue;
    hmap_idles_t   hmap;
    hmutex_t mutex;
    uint64_t       last_update_ms;
    uintptr_t      memptr;

} ATTR_ALIGNED_LINE_CACHE;

void idleCallBack(htimer_t *timer);

idle_table_t *newIdleTable(hloop_t *loop)
{
    // assert(sizeof(struct idle_table_s) <= kCpuLineCacheSize); promotion to 128 bytes
    int64_t memsize = (int64_t) sizeof(struct idle_table_s);
    // ensure we have enough space to offset the allocation by line cache (for alignment)
    MUSTALIGN2(memsize + ((kCpuLineCacheSize + 1) / 2), kCpuLineCacheSize);
    memsize = ALIGN2(memsize + ((kCpuLineCacheSize + 1) / 2), kCpuLineCacheSize);

    // check for overflow
    if (memsize < (int64_t) sizeof(struct idle_table_s))
    {
        fprintf(stderr, "buffer size out of range");
        exit(1);
    }

    // allocate memory, placing idle_table_t at a line cache address boundary
    uintptr_t ptr = (uintptr_t) globalMalloc(memsize);

    // align c to line cache boundary
    MUSTALIGN2(ptr, kCpuLineCacheSize);

    idle_table_t *newtable = (idle_table_t *) ALIGN2(ptr, kCpuLineCacheSize); // NOLINT

    *newtable = (idle_table_t){.memptr         = ptr,
                               .loop           = loop,
                               .idle_handle    = htimer_add(loop, idleCallBack, 1000, INFINITE),
                               .hqueue         = heapq_idles_t_with_capacity(kVecCap),
                               .hmap           = hmap_idles_t_with_capacity(kVecCap),
                               .last_update_ms = hloop_now_ms(loop)};

    hmutex_init(&(newtable->mutex));
    hevent_set_userdata(newtable->idle_handle, newtable);
    return newtable;
}

idle_item_t *newIdleItem(idle_table_t *self, hash_t key, void *userdata, ExpireCallBack cb, tid_t tid,
                         uint64_t age_ms)
{
    assert(self);
    idle_item_t *item = globalMalloc(sizeof(idle_item_t));
    hmutex_lock(&(self->mutex));

    *item = (idle_item_t){.expire_at_ms = hloop_now_ms(getWorkerLoop(tid)) + age_ms,
                          .hash         = key,
                          .tid          = tid,
                          .userdata     = userdata,
                          .cb           = cb,
                          .table        = self};

    if (! hmap_idles_t_insert(&(self->hmap), item->hash, item).inserted)
    {
        // hash is already in the table !
        hmutex_unlock(&(self->mutex));
        globalFree(item);
        return NULL;
    }
    heapq_idles_t_push(&(self->hqueue), item);
    hmutex_unlock(&(self->mutex));
    return item;
}

void keepIdleItemForAtleast(idle_table_t *self, idle_item_t *item, uint64_t age_ms)
{
    if (item->removed)
    {
        return;
    }
    item->expire_at_ms = self->last_update_ms + age_ms;

    hmutex_lock(&(self->mutex));
    heapq_idles_t_make_heap(&self->hqueue);
    hmutex_unlock(&(self->mutex));
}

idle_item_t *getIdleItemByHash(tid_t tid, idle_table_t *self, hash_t key)
{
    hmutex_lock(&(self->mutex));

    hmap_idles_t_iter find_result = hmap_idles_t_find(&(self->hmap), key);
    if (find_result.ref == hmap_idles_t_end(&(self->hmap)).ref || find_result.ref->second->tid != tid)
    {
        hmutex_unlock(&(self->mutex));
        return NULL;
    }
    hmutex_unlock(&(self->mutex));
    return (find_result.ref->second);
}

bool removeIdleItemByHash(tid_t tid, idle_table_t *self, hash_t key)
{
    hmutex_lock(&(self->mutex));
    hmap_idles_t_iter find_result = hmap_idles_t_find(&(self->hmap), key);
    if (find_result.ref == hmap_idles_t_end(&(self->hmap)).ref || find_result.ref->second->tid != tid)
    {
        hmutex_unlock(&(self->mutex));
        return false;
    }
    idle_item_t *item = (find_result.ref->second);
    hmap_idles_t_erase_at(&(self->hmap), find_result);
    item->removed = true;
    // heapq_idles_t_make_heap(&self->hqueue);

    hmutex_unlock(&(self->mutex));
    return true;
}

static void beforeCloseCallBack(hevent_t *ev)
{
    idle_item_t *item = hevent_userdata(ev);
    if (! item->removed)
    {
        if (item->expire_at_ms > hloop_now_ms(getWorkerLoop(item->tid)))
        {
            hmutex_lock(&(item->table->mutex));
            heapq_idles_t_push(&(item->table->hqueue), item);
            hmutex_unlock(&(item->table->mutex));
            return;
        }

        uint64_t old_expire_at_ms = item->expire_at_ms;

        if (item->cb)
        {
            item->cb(item);
        }
        
        if (old_expire_at_ms != item->expire_at_ms && item->expire_at_ms > hloop_now_ms(getWorkerLoop(item->tid)))
        {
            hmutex_lock(&(item->table->mutex));
            heapq_idles_t_push(&(item->table->hqueue), item);
            hmutex_unlock(&(item->table->mutex));
        }
        else
        {
            bool removal_result = removeIdleItemByHash(item->tid, item->table, item->hash);
            assert(removal_result);
            (void) removal_result;
            globalFree(item);
        }
    }
    else
    {
        globalFree(item);
    }
}

void idleCallBack(htimer_t *timer)
{
    idle_table_t  *self  = hevent_userdata(timer);
    const uint64_t now   = hloop_now_ms(self->loop);
    self->last_update_ms = now;
    hmutex_lock(&(self->mutex));

    while (heapq_idles_t_size(&(self->hqueue)) > 0)
    {
        idle_item_t *item = *heapq_idles_t_top(&(self->hqueue));

        if (item->expire_at_ms <= now)
        {
            heapq_idles_t_pop(&(self->hqueue));

            if (item->removed)
            {
                // already removed
                globalFree(item);
            }
            else
            {
                // destruction must happen on other thread
                // hmap_idles_t_erase(&(self->hmap), item->hash);
                hevent_t ev;
                memset(&ev, 0, sizeof(ev));
                ev.loop = getWorkerLoop(item->tid);
                ev.cb   = beforeCloseCallBack;
                hevent_set_userdata(&ev, item);
                hloop_post_event(getWorkerLoop(item->tid), &ev);
            }
        }
        else
        {
            break;
        }
    }
    hmutex_unlock(&(self->mutex));
}

void destroyIdleTable(idle_table_t *self)
{
    htimer_del(self->idle_handle);
    heapq_idles_t_drop(&self->hqueue);
    hmap_idles_t_drop(&self->hmap);
    hmutex_destroy(&self->mutex);
    globalFree((void *) (self->memptr)); // NOLINT
}
