#include "structure.h"

#include "loggers/network_logger.h"

#include <limits.h>
#include <stdarg.h>

typedef struct httpclient_h1_response_info_s
{
    int     status_code;
    bool    transfer_chunked;
    bool    connection_upgrade;
    bool    upgrade_h2c;
    bool    has_content_length;
    int64_t content_length;
} httpclient_h1_response_info_t;

static bool parseContentLength(const char *value, int64_t *out)
{
    if (value == NULL || out == NULL)
    {
        return false;
    }

    while (*value == ' ' || *value == '\t')
    {
        ++value;
    }

    if (*value == '\0')
    {
        return false;
    }

    char               *endp = NULL;
    unsigned long long  v    = strtoull(value, &endp, 10);
    if (endp == value)
    {
        return false;
    }

    while (endp != NULL && (*endp == ' ' || *endp == '\t'))
    {
        ++endp;
    }

    if (endp == NULL || *endp != '\0')
    {
        return false;
    }

    if (v > (unsigned long long) INT64_MAX)
    {
        return false;
    }

    *out = (int64_t) v;
    return true;
}

static bool appendHeaderFmt(char *buf, size_t cap, int *offset, const char *fmt, ...)
{
    if (buf == NULL || offset == NULL || fmt == NULL || *offset < 0 || (size_t) *offset >= cap)
    {
        return false;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + *offset, cap - (size_t) *offset, fmt, args);
    va_end(args);

    if (written < 0)
    {
        return false;
    }

    if ((size_t) written >= cap - (size_t) *offset)
    {
        return false;
    }

    *offset += written;
    return true;
}

static bool sendBytesUp(tunnel_t *t, line_t *l, const void *data, uint32_t len)
{
    if (len == 0)
    {
        return true;
    }

    buffer_pool_t *pool      = lineGetBufferPool(l);
    uint32_t       max_chunk = bufferpoolGetLargeBufferSize(pool);
    if (max_chunk == 0)
    {
        return false;
    }

    const uint8_t *ptr = (const uint8_t *) data;
    uint32_t       rem = len;

    while (rem > 0)
    {
        uint32_t chunk = min(rem, max_chunk);
        sbuf_t  *buf   = allocBufferForLength(l, chunk);

        sbufSetLength(buf, chunk);
        sbufWriteLarge(buf, ptr, chunk);

        if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, buf))
        {
            return false;
        }

        ptr += chunk;
        rem -= chunk;
    }

    return true;
}

static bool sendTextUp(tunnel_t *t, line_t *l, const char *text)
{
    return sendBytesUp(t, l, text, (uint32_t) strlen(text));
}

bool httpclientTransportSendHttp1RequestHeaders(tunnel_t *t, line_t *l, bool upgrade_to_h2)
{
    httpclient_tstate_t *ts = tunnelGetState(t);

    char host_line[512];
    int  host_len = 0;
    if (ts->host_port == 0 || ts->host_port == kHttpClientDefaultHttp1Port || ts->host_port == kHttpClientDefaultHttpsPort)
    {
        host_len = snprintf(host_line, sizeof(host_line), "%s", ts->host);
    }
    else
    {
        host_len = snprintf(host_line, sizeof(host_line), "%s:%d", ts->host, ts->host_port);
    }

    if (host_len <= 0 || (size_t) host_len >= sizeof(host_line))
    {
        LOGE("HttpClient: host header is too large");
        return false;
    }

    char *header_buf = memoryAllocate(kHttpClientMaxHeaderBytes);
    int  offset = 0;

    if (! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "%s %s HTTP/1.1\r\n", ts->method, ts->path) ||
        ! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "Host: %s\r\n", host_line) ||
        ! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "User-Agent: WaterWall/1.x\r\n") ||
        ! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "Accept: */*\r\n"))
    {
        LOGE("HttpClient: request headers are too large");
        memoryFree(header_buf);
        return false;
    }

    if (upgrade_to_h2)
    {
        if (ts->upgrade_settings_b64 == NULL || ts->upgrade_settings_payload == NULL || ts->upgrade_settings_payload_len == 0)
        {
            LOGE("HttpClient: HTTP2-Settings is not initialized");
            memoryFree(header_buf);
            return false;
        }

        if (! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "Connection: Upgrade, HTTP2-Settings\r\n") ||
            ! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "Upgrade: h2c\r\n") ||
            ! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "HTTP2-Settings: %s\r\n",
                              ts->upgrade_settings_b64))
        {
            LOGE("HttpClient: request headers are too large");
            memoryFree(header_buf);
            return false;
        }
    }
    else
    {
        if (! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset,
                              "Connection: keep-alive\r\nTransfer-Encoding: chunked\r\n"))
        {
            LOGE("HttpClient: request headers are too large");
            memoryFree(header_buf);
            return false;
        }
    }

    if (ts->content_type != kContentTypeNone && ts->content_type != kContentTypeUndefined)
    {
        if (! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "Content-Type: %s\r\n",
                              httpContentTypeStr(ts->content_type)))
        {
            LOGE("HttpClient: request headers are too large");
            memoryFree(header_buf);
            return false;
        }
    }

    if (cJSON_IsObject(ts->headers))
    {
        cJSON *header = NULL;
        cJSON_ArrayForEach(header, ts->headers)
        {
            if (! cJSON_IsString(header) || header->valuestring == NULL || header->string == NULL)
            {
                continue;
            }

            if (! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "%s: %s\r\n", header->string,
                                  header->valuestring))
            {
                LOGE("HttpClient: request headers are too large");
                memoryFree(header_buf);
                return false;
            }
        }
    }

    if (! appendHeaderFmt(header_buf, kHttpClientMaxHeaderBytes, &offset, "\r\n"))
    {
        LOGE("HttpClient: request headers are too large");
        memoryFree(header_buf);
        return false;
    }

    bool ok = sendBytesUp(t, l, header_buf, (uint32_t) offset);
    memoryFree(header_buf);
    return ok;
}

bool httpclientTransportSendHttp1FinalChunk(tunnel_t *t, line_t *l)
{
    return sendTextUp(t, l, "0\r\n\r\n");
}

bool httpclientTransportSendHttp1ChunkedPayload(tunnel_t *t, line_t *l, sbuf_t *payload)
{
    uint32_t payload_len = sbufGetLength(payload);

    char chunk_prefix[32];
    int  prefix_len = snprintf(chunk_prefix, sizeof(chunk_prefix), "%x\r\n", payload_len);

    if (prefix_len <= 0)
    {
        return withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, payload);
    }

    if (sbufGetLeftCapacity(payload) >= (uint32_t) prefix_len)
    {
        sbufShiftLeft(payload, (uint32_t) prefix_len);
        sbufWrite(payload, chunk_prefix, (uint32_t) prefix_len);
    }
    else
    {
        if (! sendBytesUp(t, l, chunk_prefix, (uint32_t) prefix_len))
        {
            lineReuseBuffer(l, payload);
            return false;
        }
    }

    bool appended_tail = false;
    if (sbufGetMaximumWriteableSize(payload) >= 2)
    {
        uint32_t old_len = sbufGetLength(payload);
        sbufSetLength(payload, old_len + 2);
        memoryCopy((uint8_t *) sbufGetMutablePtr(payload) + old_len, "\r\n", 2);
        appended_tail = true;
    }

    if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, payload))
    {
        return false;
    }

    if (! appended_tail)
    {
        if (! sendTextUp(t, l, "\r\n"))
        {
            return false;
        }
    }

    return true;
}

static bool parseHttp1ResponseHeaders(const char *headers, httpclient_h1_response_info_t *info)
{
    if (headers == NULL || info == NULL)
    {
        return false;
    }

    *info = (httpclient_h1_response_info_t){0};

    char *tmp = stringDuplicate(headers);

    char *line_end = strstr(tmp, "\r\n");
    if (line_end == NULL)
    {
        memoryFree(tmp);
        return false;
    }

    *line_end = '\0';

    int status_code = 0;
    if (sscanf(tmp, "HTTP/%*d.%*d %d", &status_code) != 1)
    {
        memoryFree(tmp);
        return false;
    }
    info->status_code = status_code;

    char *line = line_end + 2;
    while (*line != '\0')
    {
        char *next = strstr(line, "\r\n");
        if (next == NULL)
        {
            break;
        }

        *next = '\0';

        if (*line == '\0')
        {
            break;
        }

        char *colon = strchr(line, ':');
        if (colon != NULL)
        {
            *colon = '\0';
            char *key   = line;
            char *value = colon + 1;

            while (*value == ' ' || *value == '\t')
            {
                ++value;
            }

            if (httpclientStringCaseEquals(key, "Transfer-Encoding") && httpclientStringCaseContainsToken(value, "chunked"))
            {
                info->transfer_chunked = true;
            }
            else if (httpclientStringCaseEquals(key, "Connection") && httpclientStringCaseContainsToken(value, "upgrade"))
            {
                info->connection_upgrade = true;
            }
            else if (httpclientStringCaseEquals(key, "Upgrade") && httpclientStringCaseContains(value, "h2c"))
            {
                info->upgrade_h2c = true;
            }
            else if (httpclientStringCaseEquals(key, "Content-Length"))
            {
                int64_t parsed = 0;
                if (! parseContentLength(value, &parsed))
                {
                    memoryFree(tmp);
                    return false;
                }

                if (! info->has_content_length)
                {
                    info->has_content_length = true;
                    info->content_length     = parsed;
                }
                else if (info->content_length != parsed)
                {
                    memoryFree(tmp);
                    return false;
                }
            }
        }

        line = next + 2;
    }

    memoryFree(tmp);
    return true;
}

static bool sendNghttp2Outbound(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    buffer_pool_t *pool      = lineGetBufferPool(l);
    uint32_t       max_chunk = bufferpoolGetLargeBufferSize(pool);
    if (max_chunk == 0)
    {
        return false;
    }

    while (true)
    {
        const uint8_t *data = NULL;
        nghttp2_ssize   len  = nghttp2_session_mem_send2(ls->session, &data);

        if (len < 0)
        {
            LOGE("HttpClient: nghttp2_session_mem_send2 failed");
            return false;
        }

        if (len == 0)
        {
            break;
        }

        if ((uint64_t) len > UINT32_MAX)
        {
            LOGE("HttpClient: outgoing HTTP/2 frame exceeds buffer limits");
            return false;
        }

        uint32_t        rem = (uint32_t) len;
        const uint8_t  *ptr = data;
        while (rem > 0)
        {
            uint32_t chunk = min(rem, max_chunk);
            sbuf_t  *buf   = allocBufferForLength(l, chunk);

            sbufSetLength(buf, chunk);
            sbufWriteLarge(buf, ptr, chunk);

            if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, buf))
            {
                return false;
            }

            ptr += chunk;
            rem -= chunk;
        }
    }

    return true;
}

static int httpclientOnHeaderCallback(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
                                      size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
                                      void *userdata)
{
    discard session;
    discard frame;
    discard name;
    discard namelen;
    discard value;
    discard valuelen;
    discard flags;
    discard userdata;
    return 0;
}

static int httpclientOnDataChunkRecvCallback(nghttp2_session *session, uint8_t flags, int32_t stream_id,
                                             const uint8_t *data, size_t len, void *userdata)
{
    discard session;
    discard flags;

    if (userdata == NULL || len == 0)
    {
        return 0;
    }

    httpclient_lstate_t *ls = (httpclient_lstate_t *) userdata;

    if (ls->h2_stream_id != 0 && stream_id != ls->h2_stream_id)
    {
        return 0;
    }

    buffer_pool_t *pool      = lineGetBufferPool(ls->line);
    uint32_t       max_chunk = bufferpoolGetLargeBufferSize(pool);
    if (max_chunk == 0)
    {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }

    uint32_t       rem = (uint32_t) len;
    const uint8_t *ptr = data;
    while (rem > 0)
    {
        uint32_t chunk = min(rem, max_chunk);
        sbuf_t  *buf   = allocBufferForLength(ls->line, chunk);
        sbufSetLength(buf, chunk);
        sbufWriteLarge(buf, ptr, chunk);

        contextqueuePush(&ls->events_down, contextCreatePayload(ls->line, buf));
        ptr += chunk;
        rem -= chunk;
    }

    return 0;
}

static int httpclientOnFrameRecvCallback(nghttp2_session *session, const nghttp2_frame *frame, void *userdata)
{
    discard session;

    if (userdata == NULL)
    {
        return 0;
    }

    httpclient_lstate_t *ls = (httpclient_lstate_t *) userdata;

    if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_RESPONSE)
    {
        if (ls->h2_stream_id == 0)
        {
            ls->h2_stream_id = frame->hd.stream_id;
        }

        if (frame->hd.stream_id == ls->h2_stream_id)
        {
            ls->h2_headers_received = true;
        }
    }

    if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) == NGHTTP2_FLAG_END_STREAM && frame->hd.stream_id == ls->h2_stream_id)
    {
        ls->response_complete = true;
    }

    return 0;
}

static int httpclientOnStreamClosedCallback(nghttp2_session *session, int32_t stream_id, uint32_t error_code,
                                            void *userdata)
{
    discard session;
    discard error_code;

    if (userdata == NULL)
    {
        return 0;
    }

    httpclient_lstate_t *ls = (httpclient_lstate_t *) userdata;

    if (stream_id == ls->h2_stream_id)
    {
        ls->response_complete = true;
    }

    return 0;
}

static bool httpclientSubmitHttp2RequestHeaders(httpclient_tstate_t *ts, httpclient_lstate_t *ls, int32_t *stream_id_out)
{
    if (ts == NULL || ls == NULL || stream_id_out == NULL)
    {
        return false;
    }

    char authority[512];
    int  authority_len = 0;
    if (ts->host_port == 0 || ts->host_port == kHttpClientDefaultHttp1Port || ts->host_port == kHttpClientDefaultHttpsPort)
    {
        authority_len = snprintf(authority, sizeof(authority), "%s", ts->host);
    }
    else
    {
        authority_len = snprintf(authority, sizeof(authority), "%s:%d", ts->host, ts->host_port);
    }

    if (authority_len <= 0 || (size_t) authority_len >= sizeof(authority))
    {
        LOGE("HttpClient: authority header is too large");
        return false;
    }

    nghttp2_nv nvs[16];
    int        nvlen = 0;

    nvs[nvlen++] = (nghttp2_nv) {.name = (uint8_t *) ":method",
                                 .value = (uint8_t *) ts->method,
                                 .namelen = 7,
                                 .valuelen = stringLength(ts->method),
                                 .flags = NGHTTP2_NV_FLAG_NONE};

    nvs[nvlen++] = (nghttp2_nv) {.name = (uint8_t *) ":path",
                                 .value = (uint8_t *) ts->path,
                                 .namelen = 5,
                                 .valuelen = stringLength(ts->path),
                                 .flags = NGHTTP2_NV_FLAG_NONE};

    nvs[nvlen++] = (nghttp2_nv) {.name = (uint8_t *) ":scheme",
                                 .value = (uint8_t *) ts->scheme,
                                 .namelen = 7,
                                 .valuelen = stringLength(ts->scheme),
                                 .flags = NGHTTP2_NV_FLAG_NONE};

    nvs[nvlen++] = (nghttp2_nv) {.name = (uint8_t *) ":authority",
                                 .value = (uint8_t *) authority,
                                 .namelen = 10,
                                 .valuelen = stringLength(authority),
                                 .flags = NGHTTP2_NV_FLAG_NONE};

    if (ts->content_type != kContentTypeNone && ts->content_type != kContentTypeUndefined)
    {
        const char *ctype = httpContentTypeStr(ts->content_type);
        nvs[nvlen++]      = (nghttp2_nv) {.name = (uint8_t *) "content-type",
                                          .value = (uint8_t *) ctype,
                                          .namelen = 12,
                                          .valuelen = stringLength(ctype),
                                          .flags = NGHTTP2_NV_FLAG_NONE};
    }

    if (cJSON_IsObject(ts->headers))
    {
        cJSON *header = NULL;
        cJSON_ArrayForEach(header, ts->headers)
        {
            if (! cJSON_IsString(header) || header->valuestring == NULL || header->string == NULL)
            {
                continue;
            }

            if (header->string[0] == ':')
            {
                continue;
            }

            if (nvlen >= (int) ARRAY_SIZE(nvs))
            {
                break;
            }

            nvs[nvlen++] = (nghttp2_nv) {.name = (uint8_t *) header->string,
                                          .value = (uint8_t *) header->valuestring,
                                          .namelen = stringLength(header->string),
                                          .valuelen = stringLength(header->valuestring),
                                          .flags = NGHTTP2_NV_FLAG_NONE};
        }
    }

    int32_t stream_id = nghttp2_submit_headers(ls->session, NGHTTP2_FLAG_END_HEADERS, -1, NULL, nvs, (size_t) nvlen, NULL);
    if (stream_id <= 0)
    {
        LOGE("HttpClient: nghttp2_submit_headers failed");
        return false;
    }

    *stream_id_out = stream_id;
    return true;
}

bool httpclientTransportEnsureHttp2Session(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    if (ls->session != NULL)
    {
        return true;
    }

    httpclient_tstate_t *ts = tunnelGetState(t);

    nghttp2_session_callbacks_set_on_header_callback(ts->cbs, httpclientOnHeaderCallback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(ts->cbs, httpclientOnDataChunkRecvCallback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(ts->cbs, httpclientOnFrameRecvCallback);
    nghttp2_session_callbacks_set_on_stream_close_callback(ts->cbs, httpclientOnStreamClosedCallback);

    if (nghttp2_session_client_new3(&ls->session, ts->cbs, ls, ts->ngoptions, NULL) != 0)
    {
        LOGE("HttpClient: nghttp2_session_client_new3 failed");
        return false;
    }

    nghttp2_settings_entry settings[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 1},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, (1U << 20)},
        {NGHTTP2_SETTINGS_MAX_FRAME_SIZE, (uint32_t) kHttpClientHttp2FrameBytes}
    };

    if (nghttp2_submit_settings(ls->session, NGHTTP2_FLAG_NONE, settings, ARRAY_SIZE(settings)) != 0)
    {
        LOGE("HttpClient: nghttp2_submit_settings failed");
        return false;
    }

    int32_t stream_id = 0;
    if (! httpclientSubmitHttp2RequestHeaders(ts, ls, &stream_id))
    {
        return false;
    }

    ls->h2_stream_id  = stream_id;
    ls->runtime_proto = kHttpClientRuntimeHttp2;

    return sendNghttp2Outbound(t, l, ls);
}

bool httpclientTransportHandleUpgradeAccepted(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    httpclient_tstate_t *ts = tunnelGetState(t);

    if (ts->upgrade_settings_payload == NULL || ts->upgrade_settings_payload_len == 0)
    {
        LOGE("HttpClient: upgrade settings payload is missing");
        return false;
    }

    if (bufferqueueGetBufCount(&ls->pending_up) > 0)
    {
        LOGE("HttpClient: h2c upgrade does not support request body payloads on the original HTTP/1.1 request");
        return false;
    }

    if (ls->session == NULL)
    {
        nghttp2_session_callbacks_set_on_header_callback(ts->cbs, httpclientOnHeaderCallback);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(ts->cbs, httpclientOnDataChunkRecvCallback);
        nghttp2_session_callbacks_set_on_frame_recv_callback(ts->cbs, httpclientOnFrameRecvCallback);
        nghttp2_session_callbacks_set_on_stream_close_callback(ts->cbs, httpclientOnStreamClosedCallback);

        if (nghttp2_session_client_new3(&ls->session, ts->cbs, ls, ts->ngoptions, NULL) != 0)
        {
            LOGE("HttpClient: nghttp2_session_client_new3 failed");
            return false;
        }
    }

    int head_request = (ts->method_enum == kHttpHead) ? 1 : 0;
    if (nghttp2_session_upgrade2(ls->session, ts->upgrade_settings_payload, ts->upgrade_settings_payload_len, head_request,
                                 ls) != 0)
    {
        LOGE("HttpClient: nghttp2_session_upgrade2 failed");
        return false;
    }

    ls->h2_stream_id  = 1;
    ls->runtime_proto = kHttpClientRuntimeHttp2;

    return sendNghttp2Outbound(t, l, ls);
}

void httpclientTransportCloseBothDirections(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    lineLock(l);

    bool close_next = ! ls->next_finished;
    bool close_prev = ! ls->prev_finished;

    ls->next_finished     = true;
    ls->prev_finished     = true;
    ls->response_complete = true;

    httpclientLinestateDestroy(ls);

    if (close_next)
    {
        tunnelNextUpStreamFinish(t, l);
    }

    if (lineIsAlive(l) && close_prev)
    {
        tunnelPrevDownStreamFinish(t, l);
    }

    lineUnlock(l);
}

bool httpclientTransportSendHttp2DataFrame(tunnel_t *t, line_t *l, httpclient_lstate_t *ls, sbuf_t *payload,
                                           bool end_stream)
{
    if (ls->h2_stream_id <= 0)
    {
        if (payload != NULL)
        {
            bufferqueuePushBack(&ls->pending_up, payload);
        }
        return true;
    }

    uint32_t payload_len = (payload == NULL) ? 0 : sbufGetLength(payload);

    if (payload_len == 0 && payload != NULL && ! end_stream)
    {
        lineReuseBuffer(l, payload);
        return true;
    }

    uint32_t remote_max = nghttp2_session_get_remote_settings(ls->session, NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
    if (remote_max < HTTP2_FRAME_HDLEN)
    {
        remote_max = (1U << 14);
    }

    buffer_pool_t *pool           = lineGetBufferPool(l);
    uint32_t       large_buf_size = bufferpoolGetLargeBufferSize(pool);
    if (large_buf_size == 0)
    {
        if (payload != NULL)
        {
            lineReuseBuffer(l, payload);
        }
        return false;
    }

    uint32_t frame_limit = min((uint32_t) kHttpClientHttp2FrameBytes, remote_max);
    if (frame_limit < (1U << 14))
    {
        frame_limit = (1U << 14);
    }

    bool send_empty_frame = (payload == NULL) || (payload_len == 0 && end_stream);

    if (! send_empty_frame && payload_len <= frame_limit)
    {
        http2_frame_hd frame = {.length    = payload_len,
                                .type      = kHttP2Data,
                                .flags     = end_stream ? kHttP2FlagEndStream : kHttP2FlagNone,
                                .stream_id = (unsigned int) ls->h2_stream_id};

        if (sbufGetLeftCapacity(payload) >= HTTP2_FRAME_HDLEN)
        {
            sbufShiftLeft(payload, HTTP2_FRAME_HDLEN);
            http2FrameHdPack(&frame, sbufGetMutablePtr(payload));
            return withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, payload);
        }

        sbuf_t *header_buf = allocBufferForLength(l, HTTP2_FRAME_HDLEN);
        sbufSetLength(header_buf, HTTP2_FRAME_HDLEN);
        http2FrameHdPack(&frame, sbufGetMutablePtr(header_buf));

        if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, header_buf))
        {
            lineReuseBuffer(l, payload);
            return false;
        }

        return withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, payload);
    }

    uint32_t       remaining   = payload_len;
    const uint8_t *payload_ptr = (payload == NULL) ? NULL : (const uint8_t *) sbufGetRawPtr(payload);

    while (remaining > 0 || send_empty_frame)
    {
        uint32_t frame_payload = send_empty_frame ? 0 : min(remaining, min(frame_limit, large_buf_size));
        bool     frame_end     = end_stream && (send_empty_frame || remaining == frame_payload);

        http2_frame_hd frame = {.length    = frame_payload,
                                .type      = kHttP2Data,
                                .flags     = frame_end ? kHttP2FlagEndStream : kHttP2FlagNone,
                                .stream_id = (unsigned int) ls->h2_stream_id};

        sbuf_t *header_buf = allocBufferForLength(l, HTTP2_FRAME_HDLEN);
        sbufSetLength(header_buf, HTTP2_FRAME_HDLEN);
        http2FrameHdPack(&frame, sbufGetMutablePtr(header_buf));
        if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, header_buf))
        {
            if (payload != NULL)
            {
                lineReuseBuffer(l, payload);
            }
            return false;
        }

        if (frame_payload > 0)
        {
            sbuf_t *data_buf = allocBufferForLength(l, frame_payload);
            sbufSetLength(data_buf, frame_payload);
            sbufWriteLarge(data_buf, payload_ptr, frame_payload);
            payload_ptr += frame_payload;
            remaining -= frame_payload;

            if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, data_buf))
            {
                if (payload != NULL)
                {
                    lineReuseBuffer(l, payload);
                }
                return false;
            }
        }

        if (send_empty_frame)
        {
            break;
        }
    }

    if (payload != NULL)
    {
        lineReuseBuffer(l, payload);
    }

    return true;
}

bool httpclientTransportFlushPendingUp(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    while (bufferqueueGetBufCount(&ls->pending_up) > 0)
    {
        sbuf_t *buf = bufferqueuePopFront(&ls->pending_up);

        if (ls->runtime_proto == kHttpClientRuntimeHttp2)
        {
            if (! httpclientTransportSendHttp2DataFrame(t, l, ls, buf, false))
            {
                return false;
            }
        }
        else if (ls->runtime_proto == kHttpClientRuntimeHttp1)
        {
            if (! httpclientTransportSendHttp1ChunkedPayload(t, l, buf))
            {
                return false;
            }
        }
        else
        {
            bufferqueuePushFront(&ls->pending_up, buf);
            return true;
        }
    }

    return true;
}

static bool drainDownEvents(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    while (contextqueueLen(&ls->events_down) > 0)
    {
        context_t *ctx = contextqueuePop(&ls->events_down);

        lineLock(l);
        contextApplyOnPrevTunnelD(ctx, t);
        contextDestroy(ctx);

        if (! lineIsAlive(l))
        {
            lineUnlock(l);
            return false;
        }
        lineUnlock(l);
    }

    return true;
}

bool httpclientTransportFeedHttp2Input(tunnel_t *t, line_t *l, httpclient_lstate_t *ls, sbuf_t *buf)
{
    uint32_t       len = sbufGetLength(buf);
    const uint8_t *ptr = (const uint8_t *) sbufGetRawPtr(buf);

    while (len > 0)
    {
        nghttp2_ssize ret = nghttp2_session_mem_recv2(ls->session, ptr, len);
        if (ret < 0)
        {
            LOGE("HttpClient: nghttp2_session_mem_recv2 failed (%zd)", ret);
            lineReuseBuffer(l, buf);
            return false;
        }

        if (ret == 0)
        {
            LOGE("HttpClient: nghttp2_session_mem_recv2 consumed 0 bytes");
            lineReuseBuffer(l, buf);
            return false;
        }

        ptr += (size_t) ret;
        len -= (uint32_t) ret;
    }

    lineReuseBuffer(l, buf);

    if (! sendNghttp2Outbound(t, l, ls))
    {
        return false;
    }

    if (! drainDownEvents(t, l, ls))
    {
        return false;
    }

    return true;
}

bool httpclientTransportHandleHttp1ResponseHeaderPhase(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    while (! ls->h1_headers_parsed)
    {
        if (bufferstreamGetBufLen(&ls->in_stream) > kHttpClientMaxHeaderBytes)
        {
            LOGE("HttpClient: response header exceeded maximum size");
            return false;
        }

        size_t header_end = 0;
        if (! bufferstreamFindDoubleCRLF(&ls->in_stream, &header_end))
        {
            return true;
        }

        sbuf_t *header_buf = bufferstreamReadExact(&ls->in_stream, header_end);

        char *header_text = memoryAllocate(header_end + 1);
        memoryCopy(header_text, sbufGetRawPtr(header_buf), header_end);
        header_text[header_end] = '\0';

        httpclient_h1_response_info_t info;
        bool                          parsed_ok = parseHttp1ResponseHeaders(header_text, &info);

        memoryFree(header_text);
        lineReuseBuffer(l, header_buf);

        if (! parsed_ok)
        {
            LOGE("HttpClient: invalid HTTP/1.1 response headers");
            return false;
        }

        if (info.status_code == 101)
        {
            if (ls->runtime_proto == kHttpClientRuntimeWaitUpgrade && info.connection_upgrade && info.upgrade_h2c)
            {
                ls->h1_upgrade_accepted = true;
                ls->runtime_proto       = kHttpClientRuntimeAfterUpgrade;
                ls->h1_headers_parsed   = true;

                if (! httpclientTransportHandleUpgradeAccepted(t, l, ls))
                {
                    return false;
                }

                while (! bufferstreamIsEmpty(&ls->in_stream))
                {
                    sbuf_t *leftover = bufferstreamIdealRead(&ls->in_stream);
                    if (! httpclientTransportFeedHttp2Input(t, l, ls, leftover))
                    {
                        return false;
                    }
                }

                return httpclientTransportFlushPendingUp(t, l, ls);
            }

            LOGE("HttpClient: unexpected HTTP/1.1 101 response");
            return false;
        }

        if (info.status_code >= 100 && info.status_code < 200)
        {
            continue;
        }

        ls->runtime_proto       = kHttpClientRuntimeHttp1;
        ls->h1_headers_parsed   = true;
        ls->h1_response_chunked = info.transfer_chunked;

        if (info.transfer_chunked && info.has_content_length)
        {
            LOGE("HttpClient: invalid HTTP/1.1 response (both Transfer-Encoding and Content-Length)");
            return false;
        }

        bool response_has_no_body = false;
        if (info.status_code == 204 || info.status_code == 304 || (info.status_code >= 100 && info.status_code < 200))
        {
            response_has_no_body = true;
        }

        httpclient_tstate_t *ts = tunnelGetState(t);
        if (ts->method_enum == kHttpHead)
        {
            response_has_no_body = true;
        }

        if (response_has_no_body)
        {
            ls->h1_body_mode      = kHttpClientH1BodyNone;
            ls->response_complete = true;
            return true;
        }

        if (info.transfer_chunked)
        {
            ls->h1_body_mode      = kHttpClientH1BodyChunked;
            ls->h1_chunk_expected = -1;
        }
        else if (info.has_content_length)
        {
            ls->h1_body_mode      = kHttpClientH1BodyContentLen;
            ls->h1_body_remaining = info.content_length;
            if (ls->h1_body_remaining == 0)
            {
                ls->response_complete = true;
                return true;
            }
        }
        else
        {
            ls->h1_body_mode = kHttpClientH1BodyUntilClose;
        }

        return httpclientTransportFlushPendingUp(t, l, ls);
    }

    return true;
}

static bool parseChunkSizeLine(sbuf_t *line_buf, uint64_t *chunk_len)
{
    size_t raw_len = sbufGetLength(line_buf);
    if (raw_len < 2)
    {
        return false;
    }

    size_t line_len = raw_len - 2;
    char  *line     = memoryAllocate(line_len + 1);
    memoryCopy(line, sbufGetRawPtr(line_buf), line_len);
    line[line_len] = '\0';

    char *semi = strchr(line, ';');
    if (semi != NULL)
    {
        *semi = '\0';
    }

    char *endp                = NULL;
    unsigned long long parsed = strtoull(line, &endp, 16);
    while (endp != NULL && (*endp == ' ' || *endp == '\t'))
    {
        ++endp;
    }

    bool ok = (endp != NULL && endp != line && *endp == '\0');

    memoryFree(line);

    if (! ok)
    {
        return false;
    }

    *chunk_len = (uint64_t) parsed;
    return true;
}

bool httpclientTransportDrainHttp1ChunkedBody(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    while (true)
    {
        if (ls->h1_chunk_expected < 0)
        {
            size_t line_end = 0;
            if (! bufferstreamFindCRLF(&ls->in_stream, &line_end))
            {
                return true;
            }

            sbuf_t *line_buf = bufferstreamReadExact(&ls->in_stream, line_end + 2);

            uint64_t chunk_len = 0;
            bool     ok        = parseChunkSizeLine(line_buf, &chunk_len);
            lineReuseBuffer(l, line_buf);

            if (! ok || chunk_len > (uint64_t) INT64_MAX)
            {
                LOGE("HttpClient: invalid chunked size line");
                return false;
            }

            ls->h1_chunk_expected = (int64_t) chunk_len;

            if (ls->h1_chunk_expected == 0)
            {
                while (true)
                {
                    size_t trailer_line_end = 0;
                    if (! bufferstreamFindCRLF(&ls->in_stream, &trailer_line_end))
                    {
                        return true;
                    }

                    sbuf_t *trailer_line = bufferstreamReadExact(&ls->in_stream, trailer_line_end + 2);
                    bool    done         = (trailer_line_end == 0);
                    lineReuseBuffer(l, trailer_line);

                    if (done)
                    {
                        if (! ls->prev_finished)
                        {
                            ls->response_complete = true;
                        }
                        return true;
                    }
                }
            }
        }

        if (ls->h1_chunk_expected > 0)
        {
            uint64_t required = (uint64_t) ls->h1_chunk_expected + 2ULL;
            if (bufferstreamGetBufLen(&ls->in_stream) < required)
            {
                return true;
            }

            sbuf_t *chunk_with_tail = bufferstreamReadExact(&ls->in_stream, (size_t) required);

            uint32_t full_len = sbufGetLength(chunk_with_tail);
            if (full_len < 2)
            {
                lineReuseBuffer(l, chunk_with_tail);
                return false;
            }

            uint8_t *ptr = (uint8_t *) sbufGetMutablePtr(chunk_with_tail);
            if (ptr[full_len - 2] != '\r' || ptr[full_len - 1] != '\n')
            {
                lineReuseBuffer(l, chunk_with_tail);
                LOGE("HttpClient: invalid chunked frame tail");
                return false;
            }

            sbufSetLength(chunk_with_tail, (uint32_t) ls->h1_chunk_expected);

            if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, chunk_with_tail))
            {
                return false;
            }

            ls->h1_chunk_expected = -1;
            continue;
        }
    }
}

bool httpclientTransportDrainHttp1Body(tunnel_t *t, line_t *l, httpclient_lstate_t *ls)
{
    if (ls->prev_finished || ls->h1_body_mode == kHttpClientH1BodyNone)
    {
        return true;
    }

    if (ls->h1_body_mode == kHttpClientH1BodyChunked)
    {
        return httpclientTransportDrainHttp1ChunkedBody(t, l, ls);
    }

    if (ls->h1_body_mode == kHttpClientH1BodyUntilClose)
    {
        while (! bufferstreamIsEmpty(&ls->in_stream))
        {
            sbuf_t *buf = bufferstreamIdealRead(&ls->in_stream);
            if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, buf))
            {
                return false;
            }
        }
        return true;
    }

    while (ls->h1_body_remaining > 0)
    {
        size_t available = bufferstreamGetBufLen(&ls->in_stream);
        if (available == 0)
        {
            return true;
        }

        uint64_t to_read64 = min((uint64_t) available, (uint64_t) ls->h1_body_remaining);
        size_t   to_read   = (size_t) to_read64;
        sbuf_t  *buf       = bufferstreamReadExact(&ls->in_stream, to_read);

        ls->h1_body_remaining -= (int64_t) to_read;

        if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, buf))
        {
            return false;
        }
    }

    if (ls->h1_body_remaining == 0 && ! ls->prev_finished)
    {
        ls->response_complete = true;
        return true;
    }

    return true;
}
