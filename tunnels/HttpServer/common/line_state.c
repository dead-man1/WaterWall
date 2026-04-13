#include "structure.h"

#include "loggers/network_logger.h"

void httpserverLinestateInitialize(httpserver_lstate_t *ls, tunnel_t *t, line_t *l)
{
    *ls = (httpserver_lstate_t){.tunnel                   = t,
                                .line                     = l,
                                .session                  = NULL,
                                .in_stream                = bufferstreamCreate(lineGetBufferPool(l), 0),
                                .pending_down             = bufferqueueCreate(kHttpServerBufferQueueCap),
                                .events_up                = contextqueueCreate(),
                                .runtime_proto            = kHttpServerRuntimeUnknown,
                                .h2_stream_id             = 0,
                                .h1_chunk_expected        = -1,
                                .h1_body_remaining        = 0,
                                .h1_headers_parsed        = false,
                                .h1_request_chunked       = false,
                                .h1_request_finished      = false,
                                .h1_response_headers_sent = false,
                                .h1_body_mode             = kHttpServerH1BodyNone,
                                .h2_response_headers_sent = false,
                                .h2_request_finished      = false,
                                .fin_sent                 = false,
                                .prev_finished            = false,
                                .next_finished            = false,
                                .h2_reject_extra_streams  = true};
}

void httpserverLinestateDestroy(httpserver_lstate_t *ls)
{
    if (ls->session != NULL)
    {
        nghttp2_session_del(ls->session);
        ls->session = NULL;
    }

    bufferstreamDestroy(&ls->in_stream);
    bufferqueueDestroy(&ls->pending_down);
    contextqueueDestroy(&ls->events_up);

    memoryZeroAligned32(ls, sizeof(*ls));
}
