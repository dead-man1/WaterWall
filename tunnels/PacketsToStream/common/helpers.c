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

    if (bufferstreamGetBufLen(stream) < kHeaderSize + 1)
    {
        return false;
    }

    uint8_t packet_first_bytes[kHeaderSize];
    bufferstreamViewBytesAt(stream, 0, packet_first_bytes, kHeaderSize);

    uint16_t total_packet_size_network;
    sbufByteCopy(&total_packet_size_network, packet_first_bytes, (uint32_t) sizeof(total_packet_size_network));
    uint16_t total_packet_size = ntohs(total_packet_size_network);
    
    if (total_packet_size < 1 ||  ((uint32_t) (total_packet_size  + kHeaderSize)) > (uint32_t) bufferstreamGetBufLen(stream))
    {
        return false;
    }

    // Read the complete packet (header + payload)
    *packet_out = bufferstreamReadExact(stream, kHeaderSize + total_packet_size);
    sbufShiftRight(*packet_out, kHeaderSize);

    return true;
}

void packetstostreamRecreateOutputLineTask(tunnel_t *t, line_t *packet_line)
{
    packetstostream_lstate_t *ls = lineGetState(packet_line, t);

    if (! ls->recreate_scheduled)
    {
        return;
    }

    ls->recreate_scheduled = false;
    packetstostreamEnsureOutputLine(t, packet_line, ls);
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

    if (ls->recreate_scheduled)
    {
        return NULL;
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
