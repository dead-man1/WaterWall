#include "structure.h"

#include "loggers/network_logger.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>

typedef struct httpserver_h1_request_info_s
{
    char method[32];
    char path[2048];
    char host[512];
    char http2_settings[512];

    bool    transfer_chunked;
    bool    connection_upgrade;
    bool    connection_http2_settings;
    bool    upgrade_h2c;
    bool    has_http2_settings;
    bool    has_content_length;
    int64_t content_length;
} httpserver_h1_request_info_t;

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

static bool base64UrlDecode(const char *src, uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    if (src == NULL || dst == NULL || out_len == NULL)
    {
        return false;
    }

    size_t src_len = strlen(src);
    char  *cleaned = memoryAllocate(src_len + 1);
    size_t clean_len = 0;

    bool seen_padding = false;
    for (size_t i = 0; i < src_len; ++i)
    {
        char c = src[i];
        if (c == ' ' || c == '\t')
        {
            continue;
        }

        if (c == '=')
        {
            seen_padding = true;
            continue;
        }

        if (seen_padding)
        {
            memoryFree(cleaned);
            return false;
        }

        cleaned[clean_len++] = c;
    }
    cleaned[clean_len] = '\0';

    size_t i = 0;
    size_t o = 0;

    while (i < clean_len)
    {
        uint8_t in[4] = {0};
        size_t  in_len = 0;

        while (i < clean_len && in_len < 4)
        {
            char c = cleaned[i++];

            if (c >= 'A' && c <= 'Z')
            {
                in[in_len++] = (uint8_t) (c - 'A');
            }
            else if (c >= 'a' && c <= 'z')
            {
                in[in_len++] = (uint8_t) (26 + (c - 'a'));
            }
            else if (c >= '0' && c <= '9')
            {
                in[in_len++] = (uint8_t) (52 + (c - '0'));
            }
            else if (c == '-')
            {
                in[in_len++] = 62;
            }
            else if (c == '_')
            {
                in[in_len++] = 63;
            }
            else
            {
                memoryFree(cleaned);
                return false;
            }
        }

        if (in_len == 0)
        {
            break;
        }

        if (in_len == 1)
        {
            memoryFree(cleaned);
            return false;
        }

        if (o + (in_len - 1) > dst_cap)
        {
            memoryFree(cleaned);
            return false;
        }

        uint32_t v = ((uint32_t) in[0] << 18) | ((uint32_t) in[1] << 12) | ((uint32_t) in[2] << 6) | (uint32_t) in[3];
        dst[o++]   = (uint8_t) ((v >> 16) & 0xFF);
        if (in_len >= 3)
        {
            dst[o++] = (uint8_t) ((v >> 8) & 0xFF);
        }
        if (in_len == 4)
        {
            dst[o++] = (uint8_t) (v & 0xFF);
        }
    }

    memoryFree(cleaned);
    *out_len = o;
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

static size_t minSize(size_t a, size_t b)
{
    return a < b ? a : b;
}

static bool sendBytesDown(tunnel_t *t, line_t *l, const void *data, uint32_t len)
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
        sbuf_t  *buf   = httpserverAllocBufferForLength(l, chunk);

        sbufSetLength(buf, chunk);
        sbufWriteLarge(buf, ptr, chunk);

        if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, buf))
        {
            return false;
        }

        ptr += chunk;
        rem -= chunk;
    }

    return true;
}

static bool sendTextDown(tunnel_t *t, line_t *l, const char *text)
{
    return sendBytesDown(t, l, text, (uint32_t) strlen(text));
}

static const char *statusReasonPhrase(int status_code)
{
    const char *reason = httpStatusStr((enum http_status) status_code);

    if (reason == NULL || stringCompare(reason, "<unknown>") == 0)
    {
        return "OK";
    }

    return reason;
}

bool httpserverTransportSendHttp1ResponseHeaders(tunnel_t *t, line_t *l)
{
    httpserver_tstate_t *ts = tunnelGetState(t);

    char *header_buf = memoryAllocate(kHttpServerMaxHeaderBytes);
    int  offset = 0;

    if (! appendHeaderFmt(header_buf, kHttpServerMaxHeaderBytes, &offset, "HTTP/1.1 %d %s\r\n", ts->status_code,
                          statusReasonPhrase(ts->status_code)) ||
        ! appendHeaderFmt(header_buf, kHttpServerMaxHeaderBytes, &offset,
                          "Connection: close\r\nTransfer-Encoding: chunked\r\n"))
    {
        LOGE("HttpServer: response headers are too large");
        memoryFree(header_buf);
        return false;
    }

    if (ts->content_type != kContentTypeNone && ts->content_type != kContentTypeUndefined)
    {
        if (! appendHeaderFmt(header_buf, kHttpServerMaxHeaderBytes, &offset, "Content-Type: %s\r\n",
                              httpContentTypeStr(ts->content_type)))
        {
            LOGE("HttpServer: response headers are too large");
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

            if (! appendHeaderFmt(header_buf, kHttpServerMaxHeaderBytes, &offset, "%s: %s\r\n", header->string,
                                  header->valuestring))
            {
                LOGE("HttpServer: response headers are too large");
                memoryFree(header_buf);
                return false;
            }
        }
    }

    if (! appendHeaderFmt(header_buf, kHttpServerMaxHeaderBytes, &offset, "\r\n"))
    {
        LOGE("HttpServer: response headers are too large");
        memoryFree(header_buf);
        return false;
    }

    bool ok = sendBytesDown(t, l, header_buf, (uint32_t) offset);
    memoryFree(header_buf);
    return ok;
}

bool httpserverTransportSendHttp1FinalChunk(tunnel_t *t, line_t *l)
{
    return sendTextDown(t, l, "0\r\n\r\n");
}

bool httpserverTransportSendHttp1ChunkedPayload(tunnel_t *t, line_t *l, sbuf_t *payload)
{
    uint32_t payload_len = sbufGetLength(payload);

    char chunk_prefix[32];
    int  prefix_len = snprintf(chunk_prefix, sizeof(chunk_prefix), "%x\r\n", payload_len);

    if (prefix_len <= 0)
    {
        return withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, payload);
    }

    if (sbufGetLeftCapacity(payload) >= (uint32_t) prefix_len)
    {
        sbufShiftLeft(payload, (uint32_t) prefix_len);
        sbufWrite(payload, chunk_prefix, (uint32_t) prefix_len);
    }
    else
    {
        if (! sendBytesDown(t, l, chunk_prefix, (uint32_t) prefix_len))
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

    if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, payload))
    {
        return false;
    }

    if (! appended_tail)
    {
        if (! sendTextDown(t, l, "\r\n"))
        {
            return false;
        }
    }

    return true;
}

static bool hostMatchesExpected(const char *expected, const char *actual)
{
    if (expected == NULL || expected[0] == '\0')
    {
        return true;
    }

    if (actual == NULL || actual[0] == '\0')
    {
        return false;
    }

    if (httpserverStringCaseEquals(expected, actual))
    {
        return true;
    }

    const char *colon = strchr(actual, ':');
    if (colon == NULL)
    {
        return false;
    }

    size_t host_len = (size_t) (colon - actual);
    size_t exp_len  = strlen(expected);

    if (host_len != exp_len)
    {
        return false;
    }

    for (size_t i = 0; i < host_len; i++)
    {
        if ((char) tolower((unsigned char) actual[i]) != (char) tolower((unsigned char) expected[i]))
        {
            return false;
        }
    }

    return true;
}

static bool parseHttp1RequestHeaders(const char *headers, httpserver_h1_request_info_t *info)
{
    if (headers == NULL || info == NULL)
    {
        return false;
    }

    *info = (httpserver_h1_request_info_t){0};

    char *tmp = stringDuplicate(headers);

    char *line_end = strstr(tmp, "\r\n");
    if (line_end == NULL)
    {
        memoryFree(tmp);
        return false;
    }

    *line_end = '\0';

    char method[sizeof(info->method)] = {0};
    char path[sizeof(info->path)]     = {0};

    if (sscanf(tmp, "%31s %2047s HTTP/%*d.%*d", method, path) != 2)
    {
        memoryFree(tmp);
        return false;
    }

    snprintf(info->method, sizeof(info->method), "%s", method);
    snprintf(info->path, sizeof(info->path), "%s", path);

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

            if (httpserverStringCaseEquals(key, "Transfer-Encoding") && httpserverStringCaseContainsToken(value, "chunked"))
            {
                info->transfer_chunked = true;
            }
            else if (httpserverStringCaseEquals(key, "Connection"))
            {
                if (httpserverStringCaseContainsToken(value, "upgrade"))
                {
                    info->connection_upgrade = true;
                }
                if (httpserverStringCaseContainsToken(value, "http2-settings"))
                {
                    info->connection_http2_settings = true;
                }
            }
            else if (httpserverStringCaseEquals(key, "Upgrade") && httpserverStringCaseContains(value, "h2c"))
            {
                info->upgrade_h2c = true;
            }
            else if (httpserverStringCaseEquals(key, "HTTP2-Settings"))
            {
                if (info->has_http2_settings)
                {
                    memoryFree(tmp);
                    return false;
                }

                info->has_http2_settings = true;
                snprintf(info->http2_settings, sizeof(info->http2_settings), "%s", value);
            }
            else if (httpserverStringCaseEquals(key, "Host"))
            {
                snprintf(info->host, sizeof(info->host), "%s", value);
            }
            else if (httpserverStringCaseEquals(key, "Content-Length"))
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

static bool validateHttp1Request(const httpserver_tstate_t *ts, const httpserver_h1_request_info_t *info)
{
    if (ts->expected_method != NULL && ts->expected_method[0] != '\0' && ! httpserverStringCaseEquals(ts->expected_method, info->method))
    {
        LOGW("HttpServer: method mismatch, expected=%s got=%s", ts->expected_method, info->method);
        return false;
    }

    if (ts->expected_path != NULL && ts->expected_path[0] != '\0' && stringCompare(ts->expected_path, info->path) != 0)
    {
        LOGW("HttpServer: path mismatch, expected=%s got=%s", ts->expected_path, info->path);
        return false;
    }

    if (! hostMatchesExpected(ts->expected_host, info->host))
    {
        LOGW("HttpServer: host mismatch, expected=%s got=%s", ts->expected_host, info->host);
        return false;
    }

    return true;
}

static bool sendNghttp2Outbound(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
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
            LOGE("HttpServer: nghttp2_session_mem_send2 failed");
            return false;
        }

        if (len == 0)
        {
            break;
        }

        if ((uint64_t) len > UINT32_MAX)
        {
            LOGE("HttpServer: outgoing HTTP/2 frame exceeds buffer limits");
            return false;
        }

        uint32_t        rem = (uint32_t) len;
        const uint8_t  *ptr = data;
        while (rem > 0)
        {
            uint32_t chunk = min(rem, max_chunk);
            sbuf_t  *buf   = httpserverAllocBufferForLength(l, chunk);
            sbufSetLength(buf, chunk);
            sbufWriteLarge(buf, ptr, chunk);

            if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, buf))
            {
                return false;
            }

            ptr += chunk;
            rem -= chunk;
        }
    }

    return true;
}

static int httpserverOnHeaderCallback(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
                                      size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
                                      void *userdata)
{
    discard session;
    discard name;
    discard namelen;
    discard value;
    discard valuelen;
    discard flags;

    if (userdata == NULL)
    {
        return 0;
    }

    httpserver_lstate_t *ls = (httpserver_lstate_t *) userdata;

    if (frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST)
    {
        return 0;
    }

    if (ls->h2_stream_id != 0 && frame->hd.stream_id != ls->h2_stream_id)
    {
        if (ls->h2_reject_extra_streams)
        {
            nghttp2_submit_rst_stream(ls->session, NGHTTP2_FLAG_NONE, frame->hd.stream_id, NGHTTP2_REFUSED_STREAM);
        }
        return 0;
    }

    return 0;
}

static int httpserverOnDataChunkRecvCallback(nghttp2_session *session, uint8_t flags, int32_t stream_id,
                                             const uint8_t *data, size_t len, void *userdata)
{
    discard session;
    discard flags;

    if (userdata == NULL || len == 0)
    {
        return 0;
    }

    httpserver_lstate_t *ls = (httpserver_lstate_t *) userdata;

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
        sbuf_t  *buf   = httpserverAllocBufferForLength(ls->line, chunk);
        sbufSetLength(buf, chunk);
        sbufWriteLarge(buf, ptr, chunk);
        contextqueuePush(&ls->events_up, contextCreatePayload(ls->line, buf));
        ptr += chunk;
        rem -= chunk;
    }

    return 0;
}

static int httpserverOnFrameRecvCallback(nghttp2_session *session, const nghttp2_frame *frame, void *userdata)
{
    discard session;

    if (userdata == NULL)
    {
        return 0;
    }

    httpserver_lstate_t *ls = (httpserver_lstate_t *) userdata;

    if (frame->hd.type == NGHTTP2_HEADERS && frame->headers.cat == NGHTTP2_HCAT_REQUEST)
    {
        if (ls->h2_stream_id == 0)
        {
            ls->h2_stream_id = frame->hd.stream_id;
        }
        else if (frame->hd.stream_id != ls->h2_stream_id)
        {
            if (ls->h2_reject_extra_streams)
            {
                nghttp2_submit_rst_stream(ls->session, NGHTTP2_FLAG_NONE, frame->hd.stream_id, NGHTTP2_REFUSED_STREAM);
            }
            return 0;
        }
    }

    if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) == NGHTTP2_FLAG_END_STREAM && frame->hd.stream_id == ls->h2_stream_id)
    {
        if (! ls->h2_request_finished)
        {
            ls->h2_request_finished = true;
            contextqueuePush(&ls->events_up, contextCreateFin(ls->line));
        }
    }

    return 0;
}

static int httpserverOnStreamClosedCallback(nghttp2_session *session, int32_t stream_id, uint32_t error_code,
                                            void *userdata)
{
    discard session;
    discard error_code;

    if (userdata == NULL)
    {
        return 0;
    }

    httpserver_lstate_t *ls = (httpserver_lstate_t *) userdata;

    if (stream_id == ls->h2_stream_id && ! ls->h2_request_finished)
    {
        ls->h2_request_finished = true;
        contextqueuePush(&ls->events_up, contextCreateFin(ls->line));
    }

    return 0;
}

bool httpserverTransportEnsureHttp2Session(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    if (ls->session != NULL)
    {
        ls->runtime_proto = kHttpServerRuntimeHttp2;
        return true;
    }

    httpserver_tstate_t *ts = tunnelGetState(t);

    nghttp2_session_callbacks_set_on_header_callback(ts->cbs, httpserverOnHeaderCallback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(ts->cbs, httpserverOnDataChunkRecvCallback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(ts->cbs, httpserverOnFrameRecvCallback);
    nghttp2_session_callbacks_set_on_stream_close_callback(ts->cbs, httpserverOnStreamClosedCallback);

    if (nghttp2_session_server_new3(&ls->session, ts->cbs, ls, ts->ngoptions, NULL) != 0)
    {
        LOGE("HttpServer: nghttp2_session_server_new3 failed");
        return false;
    }

    nghttp2_settings_entry settings[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 1},
        {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, (1U << 20)},
        {NGHTTP2_SETTINGS_MAX_FRAME_SIZE, (uint32_t) kHttpServerHttp2FrameBytes}
    };

    if (nghttp2_submit_settings(ls->session, NGHTTP2_FLAG_NONE, settings, ARRAY_SIZE(settings)) != 0)
    {
        LOGE("HttpServer: nghttp2_submit_settings failed");
        return false;
    }

    ls->runtime_proto = kHttpServerRuntimeHttp2;

    return sendNghttp2Outbound(t, l, ls);
}

static bool httpserverTransportApplyUpgradeSettingsAndOpenStream(tunnel_t *t, line_t *l, httpserver_lstate_t *ls,
                                                                 const char *h2_settings_value)
{
    uint8_t settings_payload[256];
    size_t  settings_len = 0;

    if (! base64UrlDecode(h2_settings_value, settings_payload, sizeof(settings_payload), &settings_len))
    {
        LOGE("HttpServer: invalid HTTP2-Settings header");
        return false;
    }

    if (settings_len == 0)
    {
        LOGE("HttpServer: empty HTTP2-Settings header");
        return false;
    }

    if (! httpserverTransportEnsureHttp2Session(t, l, ls))
    {
        return false;
    }

    if (nghttp2_session_upgrade2(ls->session, settings_payload, settings_len, 0, NULL) != 0)
    {
        LOGE("HttpServer: nghttp2_session_upgrade2 failed");
        return false;
    }

    {
        nghttp2_nv nvs[1];
        nvs[0] = (nghttp2_nv) {.name = (uint8_t *) ":status",
                               .value = (uint8_t *) "200",
                               .namelen = 7,
                               .valuelen = 3,
                               .flags = NGHTTP2_NV_FLAG_NONE};

        if (nghttp2_submit_headers(ls->session, NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, 1, NULL, nvs, 1,
                                   NULL) != 0)
        {
            LOGE("HttpServer: failed to submit stream-1 response after upgrade");
            return false;
        }
    }

    ls->runtime_proto = kHttpServerRuntimeHttp2;

    return sendNghttp2Outbound(t, l, ls);
}

bool httpserverTransportSubmitHttp2ResponseHeaders(tunnel_t *t, line_t *l, httpserver_lstate_t *ls, bool end_stream)
{
    httpserver_tstate_t *ts = tunnelGetState(t);

    if (ls->h2_stream_id <= 0)
    {
        return false;
    }

    char status_buf[8];
    snprintf(status_buf, sizeof(status_buf), "%d", ts->status_code);

    nghttp2_nv nvs[24];
    int        nvlen = 0;

    nvs[nvlen++] = (nghttp2_nv) {.name = (uint8_t *) ":status",
                                 .value = (uint8_t *) status_buf,
                                 .namelen = 7,
                                 .valuelen = stringLength(status_buf),
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

    uint8_t flags = NGHTTP2_FLAG_END_HEADERS;
    if (end_stream)
    {
        flags |= NGHTTP2_FLAG_END_STREAM;
    }

    if (nghttp2_submit_headers(ls->session, flags, ls->h2_stream_id, NULL, nvs, (size_t) nvlen, NULL) != 0)
    {
        LOGE("HttpServer: nghttp2_submit_headers failed");
        return false;
    }

    ls->h2_response_headers_sent = true;

    return sendNghttp2Outbound(t, l, ls);
}

bool httpserverTransportSendHttp2DataFrame(tunnel_t *t, line_t *l, httpserver_lstate_t *ls, sbuf_t *payload,
                                           bool end_stream)
{
    if (ls->h2_stream_id <= 0)
    {
        if (payload != NULL)
        {
            bufferqueuePushBack(&ls->pending_down, payload);
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

    uint32_t frame_limit = min((uint32_t) kHttpServerHttp2FrameBytes, remote_max);
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
            return withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, payload);
        }

        sbuf_t *header_buf = httpserverAllocBufferForLength(l, HTTP2_FRAME_HDLEN);
        sbufSetLength(header_buf, HTTP2_FRAME_HDLEN);
        http2FrameHdPack(&frame, sbufGetMutablePtr(header_buf));
        if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, header_buf))
        {
            lineReuseBuffer(l, payload);
            return false;
        }

        return withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, payload);
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

        sbuf_t *header_buf = httpserverAllocBufferForLength(l, HTTP2_FRAME_HDLEN);
        sbufSetLength(header_buf, HTTP2_FRAME_HDLEN);
        http2FrameHdPack(&frame, sbufGetMutablePtr(header_buf));
        if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, header_buf))
        {
            if (payload != NULL)
            {
                lineReuseBuffer(l, payload);
            }
            return false;
        }

        if (frame_payload > 0)
        {
            sbuf_t *data_buf = httpserverAllocBufferForLength(l, frame_payload);
            sbufSetLength(data_buf, frame_payload);
            sbufWriteLarge(data_buf, payload_ptr, frame_payload);
            payload_ptr += frame_payload;
            remaining -= frame_payload;

            if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, data_buf))
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

bool httpserverTransportFlushPendingDown(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    while (bufferqueueGetBufCount(&ls->pending_down) > 0)
    {
        sbuf_t *buf = bufferqueuePopFront(&ls->pending_down);

        if (ls->runtime_proto == kHttpServerRuntimeHttp2)
        {
            if (! ls->h2_response_headers_sent)
            {
                if (! httpserverTransportSubmitHttp2ResponseHeaders(t, l, ls, false))
                {
                    lineReuseBuffer(l, buf);
                    return false;
                }
            }

            if (! httpserverTransportSendHttp2DataFrame(t, l, ls, buf, false))
            {
                return false;
            }
        }
        else if (ls->runtime_proto == kHttpServerRuntimeHttp1)
        {
            if (! ls->h1_response_headers_sent)
            {
                if (! httpserverTransportSendHttp1ResponseHeaders(t, l))
                {
                    lineReuseBuffer(l, buf);
                    return false;
                }
                ls->h1_response_headers_sent = true;
            }

            if (! httpserverTransportSendHttp1ChunkedPayload(t, l, buf))
            {
                return false;
            }
        }
        else
        {
            bufferqueuePushFront(&ls->pending_down, buf);
            return true;
        }
    }

    return true;
}

static bool drainUpEvents(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    while (contextqueueLen(&ls->events_up) > 0)
    {
        context_t *ctx = contextqueuePop(&ls->events_up);

        lineLock(l);
        contextApplyOnNextTunnelU(ctx, t);
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

bool httpserverTransportFeedHttp2Input(tunnel_t *t, line_t *l, httpserver_lstate_t *ls, sbuf_t *buf)
{
    uint32_t       len = sbufGetLength(buf);
    const uint8_t *ptr = (const uint8_t *) sbufGetRawPtr(buf);

    while (len > 0)
    {
        nghttp2_ssize ret = nghttp2_session_mem_recv2(ls->session, ptr, len);
        if (ret < 0)
        {
            LOGE("HttpServer: nghttp2_session_mem_recv2 failed (%zd)", ret);
            lineReuseBuffer(l, buf);
            return false;
        }

        if (ret == 0)
        {
            LOGE("HttpServer: nghttp2_session_mem_recv2 consumed 0 bytes");
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

    if (! drainUpEvents(t, l, ls))
    {
        return false;
    }

    return true;
}

bool httpserverTransportDetectRuntimeProtocol(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    if (ls->runtime_proto != kHttpServerRuntimeUnknown)
    {
        return true;
    }

    httpserver_tstate_t *ts = tunnelGetState(t);

    if (ts->version_mode == kHttpServerVersionModeHttp1)
    {
        ls->runtime_proto = kHttpServerRuntimeHttp1;
        return true;
    }

    if (ts->version_mode == kHttpServerVersionModeHttp2)
    {
        return httpserverTransportEnsureHttp2Session(t, l, ls);
    }

    size_t len = bufferstreamGetBufLen(&ls->in_stream);
    if (len == 0)
    {
        return true;
    }

    size_t probe_len = minSize(len, HTTP2_MAGIC_LEN);

    bool magic_prefix = true;
    for (size_t i = 0; i < probe_len; i++)
    {
        uint8_t b = bufferstreamViewByteAt(&ls->in_stream, i);
        if (b != (uint8_t) HTTP2_MAGIC[i])
        {
            magic_prefix = false;
            break;
        }
    }

    if (magic_prefix)
    {
        if (len < HTTP2_MAGIC_LEN)
        {
            return true;
        }

        return httpserverTransportEnsureHttp2Session(t, l, ls);
    }

    ls->runtime_proto = kHttpServerRuntimeHttp1;
    return true;
}

bool httpserverTransportHandleHttp1RequestHeaderPhase(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    if (ls->h1_headers_parsed)
    {
        return true;
    }

    if (bufferstreamGetBufLen(&ls->in_stream) > kHttpServerMaxHeaderBytes)
    {
        LOGE("HttpServer: request header exceeded maximum size");
        return false;
    }

    size_t header_end = 0;
    if (! httpserverBufferstreamFindDoubleCRLF(&ls->in_stream, &header_end))
    {
        return true;
    }

    sbuf_t *header_buf = bufferstreamReadExact(&ls->in_stream, header_end);

    char *header_text = memoryAllocate(header_end + 1);
    memoryCopy(header_text, sbufGetRawPtr(header_buf), header_end);
    header_text[header_end] = '\0';

    httpserver_h1_request_info_t info;
    bool                         parsed_ok = parseHttp1RequestHeaders(header_text, &info);

    memoryFree(header_text);
    lineReuseBuffer(l, header_buf);

    if (! parsed_ok)
    {
        LOGE("HttpServer: invalid HTTP/1.1 request headers");
        return false;
    }

    if (info.transfer_chunked && info.has_content_length)
    {
        LOGE("HttpServer: invalid HTTP/1.1 request (both Transfer-Encoding and Content-Length)");
        return false;
    }

    httpserver_tstate_t *ts = tunnelGetState(t);
    if (! validateHttp1Request(ts, &info))
    {
        return false;
    }

    if (ts->version_mode == kHttpServerVersionModeBoth && ts->enable_upgrade && info.connection_upgrade &&
        info.connection_http2_settings && info.upgrade_h2c && info.has_http2_settings)
    {
        if (! sendTextDown(t, l, HTTP2_UPGRADE_RESPONSE))
        {
            return false;
        }

        ls->h1_headers_parsed = true;
        if (! httpserverTransportApplyUpgradeSettingsAndOpenStream(t, l, ls, info.http2_settings))
        {
            return false;
        }

        while (! bufferstreamIsEmpty(&ls->in_stream))
        {
            sbuf_t *leftover = bufferstreamIdealRead(&ls->in_stream);
            if (! httpserverTransportFeedHttp2Input(t, l, ls, leftover))
            {
                return false;
            }
        }

        return httpserverTransportFlushPendingDown(t, l, ls);
    }

    ls->runtime_proto      = kHttpServerRuntimeHttp1;
    ls->h1_headers_parsed  = true;
    ls->h1_request_chunked = info.transfer_chunked;

    if (info.transfer_chunked)
    {
        ls->h1_body_mode      = kHttpServerH1BodyChunked;
        ls->h1_chunk_expected = -1;
    }
    else if (info.has_content_length)
    {
        ls->h1_body_mode      = kHttpServerH1BodyContentLen;
        ls->h1_body_remaining = info.content_length;
        if (ls->h1_body_remaining == 0 && ! ls->next_finished)
        {
            ls->next_finished       = true;
            ls->h1_request_finished = true;
            tunnelNextUpStreamFinish(t, l);
            return true;
        }
    }
    else
    {
        ls->h1_body_mode = kHttpServerH1BodyNone;
        if (! ls->next_finished)
        {
            ls->next_finished       = true;
            ls->h1_request_finished = true;
            tunnelNextUpStreamFinish(t, l);
            return true;
        }
    }

    return httpserverTransportFlushPendingDown(t, l, ls);
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

bool httpserverTransportDrainHttp1ChunkedRequestBody(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    while (true)
    {
        if (ls->h1_chunk_expected < 0)
        {
            size_t line_end = 0;
            if (! httpserverBufferstreamFindCRLF(&ls->in_stream, &line_end))
            {
                return true;
            }

            sbuf_t *line_buf = bufferstreamReadExact(&ls->in_stream, line_end + 2);

            uint64_t chunk_len = 0;
            bool     ok        = parseChunkSizeLine(line_buf, &chunk_len);
            lineReuseBuffer(l, line_buf);

            if (! ok || chunk_len > (uint64_t) INT64_MAX)
            {
                LOGE("HttpServer: invalid chunked size line");
                return false;
            }

            ls->h1_chunk_expected = (int64_t) chunk_len;

            if (ls->h1_chunk_expected == 0)
            {
                while (true)
                {
                    size_t trailer_line_end = 0;
                    if (! httpserverBufferstreamFindCRLF(&ls->in_stream, &trailer_line_end))
                    {
                        return true;
                    }

                    sbuf_t *trailer_line = bufferstreamReadExact(&ls->in_stream, trailer_line_end + 2);
                    bool    done         = (trailer_line_end == 0);
                    lineReuseBuffer(l, trailer_line);

                    if (done)
                    {
                        if (! ls->next_finished)
                        {
                            ls->next_finished       = true;
                            ls->h1_request_finished = true;
                            tunnelNextUpStreamFinish(t, l);
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
                LOGE("HttpServer: invalid chunked frame tail");
                return false;
            }

            sbufSetLength(chunk_with_tail, (uint32_t) ls->h1_chunk_expected);

            if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, chunk_with_tail))
            {
                return false;
            }

            ls->h1_chunk_expected = -1;
            continue;
        }
    }
}

bool httpserverTransportDrainHttp1RequestBody(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    if (ls->h1_body_mode == kHttpServerH1BodyNone || ls->next_finished)
    {
        return true;
    }

    if (ls->h1_body_mode == kHttpServerH1BodyChunked)
    {
        return httpserverTransportDrainHttp1ChunkedRequestBody(t, l, ls);
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

        sbuf_t *buf = bufferstreamReadExact(&ls->in_stream, to_read);
        ls->h1_body_remaining -= (int64_t) to_read;

        if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, buf))
        {
            return false;
        }
    }

    if (ls->h1_body_remaining == 0 && ! ls->next_finished)
    {
        ls->next_finished       = true;
        ls->h1_request_finished = true;
        tunnelNextUpStreamFinish(t, l);
        return true;
    }

    return true;
}

void httpserverTransportCloseBothDirections(tunnel_t *t, line_t *l, httpserver_lstate_t *ls)
{
    bool close_next = ! ls->next_finished;
    bool close_prev = ! ls->prev_finished;

    ls->next_finished = true;
    ls->prev_finished = true;

    httpserverLinestateDestroy(ls);

    if (close_next)
    {
        tunnelNextUpStreamFinish(t, l);
    }

    if (close_prev)
    {
        tunnelPrevDownStreamFinish(t, l);
    }
}
