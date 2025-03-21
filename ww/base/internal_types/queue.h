#ifndef WW_QUEUE_H_
#define WW_QUEUE_H_

/*
 * queue
 * FIFO: push_back,pop_front
 * stack
 * LIFO: push_back,pop_back
 */

#include <assert.h> // for assert
#include <stddef.h> // for NULL
#include <stdlib.h> // for malloc,realloc,free
#include <string.h> // for memorySet,memoryMove

#include "ev_memory.h"  // for EVENTLOOP_ALLOC, EVENTLOOP_FREE

#define QUEUE_INIT_SIZE     16

// #include <deque>
// typedef std::deque<type> qtype;
#define QUEUE_DECL(type, qtype) \
struct qtype {      \
    type*   ptr;    \
    size_t  size;   \
    size_t  maxsize;\
    size_t  _offset;\
};                  \
typedef struct qtype qtype;\
\
static inline type* qtype##_data(qtype* p) {\
    return p->ptr + p->_offset;\
}\
\
static inline int qtype##_size(qtype* p) {\
    return (int) p->size;\
}\
\
static inline int qtype##_maxsize(qtype* p) {\
    return (int) p->maxsize;\
}\
\
static inline int qtype##_empty(qtype* p) {\
    return p->size == 0;\
}\
\
static inline type* qtype##_front(qtype* p) {\
    return p->size == 0 ? NULL : p->ptr + p->_offset;\
}\
\
static inline type* qtype##_back(qtype* p) {\
    return p->size == 0 ? NULL : p->ptr + p->_offset + p->size - 1;\
}\
\
static inline void qtype##_init(qtype* p, int maxsize) {\
    p->_offset = 0;\
    p->size = 0;\
    p->maxsize = (size_t)maxsize;\
    EVENTLOOP_ALLOC(p->ptr, sizeof(type) * (size_t)maxsize);\
}\
\
static inline void qtype##_clear(qtype* p) {\
    p->_offset = 0;\
    p->size = 0;\
    memorySet(p->ptr, 0, sizeof(type) * p->maxsize);\
}\
\
static inline void qtype##_cleanup(qtype* p) {\
    EVENTLOOP_FREE(p->ptr);\
    p->_offset = p->size = p->maxsize = 0;\
}\
\
static inline void qtype##_resize(qtype* p, int maxsize) {\
    if (maxsize == 0) maxsize = QUEUE_INIT_SIZE;\
    p->ptr = (type*)eventloopRealloc(p->ptr, sizeof(type) * (size_t)maxsize, sizeof(type) * p->maxsize);\
    p->maxsize = (size_t)maxsize;\
}\
\
static inline void qtype##_double_resize(qtype* p) {\
    qtype##_resize(p, (int)(p->maxsize * 2));\
}\
\
static inline void qtype##_push_back(qtype* p, type* elem) {\
    if (p->size == p->maxsize) {\
        qtype##_double_resize(p);\
    }\
    else if (p->_offset + p->size == p->maxsize) {\
        memoryMove(p->ptr, p->ptr + p->_offset, sizeof(type) * p->size);\
        p->_offset = 0;\
    }\
    p->ptr[p->_offset + p->size] = *elem;\
    p->size++;\
}\
static inline void qtype##_pop_front(qtype* p) {\
    assert(p->size > 0);\
    p->size--;\
    if (++p->_offset == p->maxsize) p->_offset = 0;\
}\
\
static inline void qtype##_pop_back(qtype* p) {\
    assert(p->size > 0);\
    p->size--;\
}\

#endif // WW_QUEUE_H_
