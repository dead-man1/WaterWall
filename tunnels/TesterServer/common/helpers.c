#include "structure.h"

#include "loggers/network_logger.h"

static const uint32_t kTesterServerChunkSizes[kTesterServerChunkCount] = {
    1U,
    2U,
    4U,
    32U,
    512U,
    1024U,
    4096U,
    32768U,
    32769U,
    16U * 1024U * 1024U,
    32U * 1024U * 1024U,
};

static const uint32_t kTesterServerPacketChunkSizes[kTesterServerChunkCount] = {
    1U,
    2U,
    4U,
    32U,
    64U,
    128U,
    256U,
    512U,
    1024U,
    1499U,
    1500U,
};

static inline const uint32_t *testerserverGetChunkTable(tunnel_t *t)
{
    testerserver_tstate_t *ts = tunnelGetState(t);

    return ts->packet_mode ? kTesterServerPacketChunkSizes : kTesterServerChunkSizes;
}

static uint8_t testerserverPatternByte(uint32_t offset, uint8_t chunk_index, wid_t wid,
                                       testerserver_direction_e direction)
{
    uint32_t value = offset;
    value ^= value >> 13;
    value *= 0x45d9f3bu;
    value ^= ((uint32_t) chunk_index + 1U) * 0x27d4eb2du;
    value ^= ((uint32_t) wid + 1U) * 0x165667b1u;
    value ^= (direction == kTesterServerDirectionResponse) ? 0xA5A5A5A5u : 0x5A5A5A5Au;
    value ^= value >> 16;
    return (uint8_t) value;
}

static void testerserverFillPayload(line_t *l, sbuf_t *buf, uint8_t chunk_index, testerserver_direction_e direction)
{
    uint32_t payload_len = sbufGetLength(buf);
    uint8_t *ptr         = sbufGetMutablePtr(buf);
    wid_t    wid         = lineGetWID(l);

    for (uint32_t i = 0; i < payload_len; ++i)
    {
        ptr[i] = testerserverPatternByte(i, chunk_index, wid, direction);
    }
}

void testerserverFail(tunnel_t *t, line_t *l, const char *reason)
{
    LOGE("TesterServer: worker %u failed: %s", (unsigned int) lineGetWID(l), reason);
    discard t;
    terminateProgram(1);
}

uint32_t testerserverGetChunkSize(tunnel_t *t, uint8_t index)
{
    assert(index < kTesterServerChunkCount);
    return testerserverGetChunkTable(t)[index];
}

uint64_t testerserverGetRemainingBytes(tunnel_t *t, uint8_t index)
{
    uint64_t remaining = 0;
    const uint32_t *chunk_sizes = testerserverGetChunkTable(t);

    for (uint8_t i = index; i < kTesterServerChunkCount; ++i)
    {
        remaining += chunk_sizes[i];
    }

    return remaining;
}

sbuf_t *testerserverCreatePayload(tunnel_t *t, line_t *l, uint8_t chunk_index, testerserver_direction_e direction)
{
    buffer_pool_t *pool        = lineGetBufferPool(l);
    uint32_t       payload_len = testerserverGetChunkSize(t, chunk_index);
    sbuf_t        *buf         = NULL;

    if (payload_len <= bufferpoolGetSmallBufferSize(pool))
    {
        buf = bufferpoolGetSmallBuffer(pool);
    }
    else if (payload_len <= bufferpoolGetLargeBufferSize(pool))
    {
        buf = bufferpoolGetLargeBuffer(pool);
    }
    else
    {
        buf = sbufCreate(payload_len);
    }

    buf = sbufReserveSpace(buf, payload_len);
    sbufSetLength(buf, payload_len);
    testerserverFillPayload(l, buf, chunk_index, direction);

    return buf;
}

bool testerserverVerifyChunk(tunnel_t *t, line_t *l, sbuf_t *buf, uint8_t chunk_index, testerserver_direction_e direction,
                             uint32_t *bad_offset, uint8_t *expected, uint8_t *actual)
{
    const uint8_t *ptr         = sbufGetRawPtr(buf);
    uint32_t       payload_len = sbufGetLength(buf);
    wid_t          wid         = lineGetWID(l);

    if (payload_len != testerserverGetChunkSize(t, chunk_index))
    {
        if (bad_offset != NULL)
        {
            *bad_offset = payload_len;
        }
        return false;
    }

    for (uint32_t i = 0; i < payload_len; ++i)
    {
        uint8_t expected_byte = testerserverPatternByte(i, chunk_index, wid, direction);
        if (ptr[i] != expected_byte)
        {
            if (bad_offset != NULL)
            {
                *bad_offset = i;
            }
            if (expected != NULL)
            {
                *expected = expected_byte;
            }
            if (actual != NULL)
            {
                *actual = ptr[i];
            }
            return false;
        }
    }

    return true;
}

void testerserverScheduleResponseSend(tunnel_t *t, line_t *l, testerserver_lstate_t *ls)
{
    testerserver_tstate_t *ts = tunnelGetState(t);

    if (ls->response_send_scheduled || ls->response_sent)
    {
        return;
    }

    if (ts->packet_mode)
    {
        if (ls->response_paused || bufferqueueGetBufCount(&ls->response_queue) == 0)
        {
            return;
        }
    }
    else if (! ls->response_ready)
    {
        return;
    }

    ls->response_send_scheduled = true;
    lineScheduleTask(l, testerserverResponseSendTask, t);
}

void testerserverResponseSendTask(tunnel_t *t, line_t *l)
{
    testerserver_lstate_t *ls = lineGetState(l, t);
    testerserver_tstate_t *ts = tunnelGetState(t);

    ls->response_send_scheduled = false;

    if (ts->packet_mode)
    {
        while (! ls->response_paused && bufferqueueGetBufCount(&ls->response_queue) > 0)
        {
            sbuf_t *buf = bufferqueuePopFront(&ls->response_queue);

            ls->response_tx_index += 1;
            if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, buf))
            {
                LOGF("TesterServer: packet line died during packet-mode response send");
                terminateProgram(1);
                return;
            }
        }

        if (ls->request_rx_index == kTesterServerChunkCount && ls->response_tx_index == kTesterServerChunkCount &&
            bufferqueueGetBufCount(&ls->response_queue) == 0)
        {
            ls->response_sent = true;
        }

        return;
    }

    while (! ls->response_paused && ls->response_tx_index < kTesterServerChunkCount)
    {
        sbuf_t *buf = testerserverCreatePayload(t, l, ls->response_tx_index, kTesterServerDirectionResponse);

        ls->response_tx_index += 1;
        tunnelPrevDownStreamPayload(t, l, buf);

        if (! lineIsAlive(l))
        {
            return;
        }
    }

    if (ls->response_tx_index == kTesterServerChunkCount)
    {
        ls->response_sent = true;
    }
}
