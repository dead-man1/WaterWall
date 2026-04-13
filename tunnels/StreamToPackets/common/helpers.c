#include "structure.h"

#include "loggers/network_logger.h"

static void streamtopacketsDropOneByte(buffer_stream_t *stream)
{
    sbuf_t *skip_buf = bufferstreamReadExact(stream, 1);
    if (skip_buf)
    {
        bufferpoolReuseBuffer(stream->pool, skip_buf);
    }
}

bool streamtopacketsReadStreamIsOverflowed(buffer_stream_t *read_stream)
{
    if (bufferstreamGetBufLen(read_stream) > kMaxBufferSize)
    {
        LOGW("StreamToPackets: read stream overflow, size: %zu, limit: %zu", bufferstreamGetBufLen(read_stream),
             (size_t) kMaxBufferSize);
        return true;
    }

    return false;
}

bool streamtopacketsTryReadIPv4Packet(buffer_stream_t *stream, sbuf_t **packet_out)
{
    assert(packet_out != NULL);
    *packet_out = NULL;

    if (bufferstreamGetBufLen(stream) < kHeaderSize + 1)
    {
        return false;
    }

    uint8_t packet_first_bytes[kHeaderSize];
    bufferstreamViewBytesAt(stream, 0, packet_first_bytes, kHeaderSize);

    uint16_t total_packet_size = ntohs(*(uint16_t *) packet_first_bytes);

    if (total_packet_size < 1 || ((uint32_t) (total_packet_size  + kHeaderSize)) > (uint32_t) bufferstreamGetBufLen(stream))
    {
        return false;
    }

    // Read the complete packet (header + payload)
    *packet_out = bufferstreamReadExact(stream, kHeaderSize + total_packet_size);
    sbufShiftRight(*packet_out, kHeaderSize);

    return true;

}


