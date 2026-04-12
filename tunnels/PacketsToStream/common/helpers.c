#include "structure.h"

#include "loggers/network_logger.h"

static void packetstostreamDropOneByte(buffer_stream_t *stream)
{
    sbuf_t *skip_buf = bufferstreamReadExact(stream, 1);
    if (skip_buf)
    {
        bufferpoolReuseBuffer(stream->pool, skip_buf);
    }
}

bool packetstostreamReadStreamIsOverflowed(buffer_stream_t *read_stream)
{
    if (bufferstreamGetBufLen(read_stream) > kMaxBufferSize)
    {
        LOGW("PacketsToStream: read stream overflow, size: %zu, limit: %zu", bufferstreamGetBufLen(read_stream),
             (size_t) kMaxBufferSize);
        return true;
    }

    return false;
}

bool packetstostreamTryReadIPv4Packet(buffer_stream_t *stream, sbuf_t **packet_out)
{
    assert(packet_out != NULL);
    *packet_out = NULL;

    while (bufferstreamGetBufLen(stream) >= IP_HLEN)
    {
        uint8_t ip_header_bytes[IP_HLEN];
        bufferstreamViewBytesAt(stream, 0, ip_header_bytes, IP_HLEN);

        uint8_t version    = (uint8_t) (ip_header_bytes[0] >> 4);
        uint8_t ihl_words  = (uint8_t) (ip_header_bytes[0] & 0x0F);
        size_t  header_len = (size_t) ihl_words * 4U;
        size_t  total_len  = ((size_t) ip_header_bytes[2] << 8U) | (size_t) ip_header_bytes[3];

        if (version != 4 || ihl_words < 5 || total_len < header_len)
        {
            LOGW("PacketsToStream: dropping invalid IPv4 framing byte while parsing stream");
            packetstostreamDropOneByte(stream);
            continue;
        }

        if (bufferstreamGetBufLen(stream) < total_len)
        {
            return false;
        }

        *packet_out = bufferstreamReadExact(stream, total_len);
        return true;
    }

    return false;
}

line_t *packetstostreamEnsureOutputLine(tunnel_t *t, line_t *packet_line, packetstostream_lstate_t *ls)
{
    if (ls->read_stream.pool == NULL)
    {
        packetstostreamLinestateInitialize(ls, lineGetBufferPool(packet_line));
    }

    if (ls->line != NULL && lineIsAlive(ls->line))
    {
        return ls->line;
    }

    ls->line   = NULL;
    ls->paused = false;
    bufferstreamEmpty(&ls->read_stream);

    line_t *new_line = lineCreate(tunnelchainGetLinePools(tunnelGetChain(t)), lineGetWID(packet_line));
    ls->line         = new_line;

    if (! withLineLocked(new_line, tunnelNextUpStreamInit, t))
    {
        if (ls->line == new_line)
        {
            ls->line = NULL;
        }
        return ls->line;
    }

    return ls->line;
}
