#include "structure.h"

#include "loggers/network_logger.h"

static void ipmanipulatorSendWithDuplicates(tunnel_t *t, line_t *l, sbuf_t *buf, LineTaskFnWithBuf forward)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);

    if (! state->trick_packet_duplicate || state->trick_packet_duplicate_count <= 0)
    {
        forward(t, l, buf);
        return;
    }

    buffer_pool_t *pool = lineGetBufferPool(l);
    bool           recalculate_checksum = lineGetRecalculateChecksum(l);

    lineLock(l);

    for (int i = 0; i < state->trick_packet_duplicate_count; ++i)
    {
        sbuf_t *dup = sbufDuplicateByPool(pool, buf);

        lineSetRecalculateChecksum(l, recalculate_checksum);
        forward(t, l, dup);

        if (! lineIsAlive(l))
        {
            reuseBuffer(buf);
            lineUnlock(l);
            return;
        }
    }

    lineSetRecalculateChecksum(l, recalculate_checksum);
    forward(t, l, buf);
    lineUnlock(l);
}

void ipmanipulatorSendUpstreamFinal(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipmanipulatorSendWithDuplicates(t, l, buf, tunnelNextUpStreamPayload);
}

void ipmanipulatorSendDownstreamFinal(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipmanipulatorSendWithDuplicates(t, l, buf, tunnelPrevDownStreamPayload);
}
