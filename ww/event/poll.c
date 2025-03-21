#include "iowatcher.h"

#ifdef EVENT_POLL
#include "wplatform.h"
#include "wdef.h"
#include "wevent.h"

#ifdef OS_WIN
#define poll        WSAPoll
#endif

#ifdef OS_LINUX
#include <sys/poll.h>
#endif

#include "array.h"
#define FDS_INIT_SIZE   64
ARRAY_DECL(struct pollfd, pollfds);

typedef struct poll_ctx_s {
    int            capacity;
    struct pollfds fds;
} poll_ctx_t;

int iowatcherInit(wloop_t* loop) {
    if (loop->iowatcher)   return 0;
    poll_ctx_t* poll_ctx;
    EVENTLOOP_ALLOC_SIZEOF(poll_ctx);
    pollfds_init(&poll_ctx->fds, FDS_INIT_SIZE);
    loop->iowatcher = poll_ctx;
    return 0;
}

int iowatcherCleanUp(wloop_t* loop) {
    if (loop->iowatcher == NULL)   return 0;
    poll_ctx_t* poll_ctx = (poll_ctx_t*)loop->iowatcher;
    pollfds_cleanup(&poll_ctx->fds);
    EVENTLOOP_FREE(loop->iowatcher);
    return 0;
}

int iowatcherAddEvent(wloop_t* loop, int fd, int events) {
    if (loop->iowatcher == NULL) {
        iowatcherInit(loop);
    }
    poll_ctx_t* poll_ctx = (poll_ctx_t*)loop->iowatcher;
    wio_t* io = loop->ios.ptr[fd];
    int idx = io->event_index[0];
    struct pollfd* pfd = NULL;
    if (idx < 0) {
        io->event_index[0] = idx = poll_ctx->fds.size;
        if (idx == poll_ctx->fds.maxsize) {
            pollfds_double_resize(&poll_ctx->fds);
        }
        poll_ctx->fds.size++;
        pfd = poll_ctx->fds.ptr + idx;
        pfd->fd = fd;
        pfd->events = 0;
        pfd->revents = 0;
    }
    else {
        pfd = poll_ctx->fds.ptr + idx;
        assert(pfd->fd == fd);
    }
    if (events & WW_READ) {
        pfd->events |= POLLIN;
    }
    if (events & WW_WRITE) {
        pfd->events |= POLLOUT;
    }
    return 0;
}

int iowatcherDelEvent(wloop_t* loop, int fd, int events) {
    poll_ctx_t* poll_ctx = (poll_ctx_t*)loop->iowatcher;
    if (poll_ctx == NULL)  return 0;
    wio_t* io = loop->ios.ptr[fd];

    int idx = io->event_index[0];
    if (idx < 0) return 0;
    struct pollfd* pfd = poll_ctx->fds.ptr + idx;
    assert(pfd->fd == fd);
    if (events & WW_READ) {
        pfd->events &= ~POLLIN;
    }
    if (events & WW_WRITE) {
        pfd->events &= ~POLLOUT;
    }
    if (pfd->events == 0) {
        pollfds_del_nomove(&poll_ctx->fds, idx);
        // NOTE: correct event_index
        if (idx < poll_ctx->fds.size) {
            wio_t* last = loop->ios.ptr[poll_ctx->fds.ptr[idx].fd];
            last->event_index[0] = idx;
        }
        io->event_index[0] = -1;
    }
    return 0;
}

int iowatcherPollEvents(wloop_t* loop, int timeout) {
    poll_ctx_t* poll_ctx = (poll_ctx_t*)loop->iowatcher;
    if (poll_ctx == NULL)  return 0;
    if (poll_ctx->fds.size == 0)   return 0;
    int npoll = poll(poll_ctx->fds.ptr, poll_ctx->fds.size, timeout);
    if (npoll < 0) {
        if (errno == EINTR) {
            return 0;
        }
        printError("poll");
        return npoll;
    }
    if (npoll == 0) return 0;
    int nevents = 0;
    for (int i = 0; i < poll_ctx->fds.size; ++i) {
        int fd = poll_ctx->fds.ptr[i].fd;
        short revents = poll_ctx->fds.ptr[i].revents;
        if (revents) {
            ++nevents;
            wio_t* io = loop->ios.ptr[fd];
            if (io) {
                if (revents & (POLLIN | POLLHUP | POLLERR)) {
                    io->revents |= WW_READ;
                }
                if (revents & (POLLOUT | POLLHUP | POLLERR)) {
                    io->revents |= WW_WRITE;
                }
                EVENT_PENDING(io);
            }
        }
        if (nevents == npoll) break;
    }
    return nevents;
}
#endif
