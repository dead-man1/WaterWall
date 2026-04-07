#include "structure.h"

#include "loggers/network_logger.h"

void disturberTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{

    disturber_tstate_t *ts = tunnelGetState(t);
    disturber_lstate_t *ls = lineGetState(l, t);

    if (ls->is_deadhang)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    if (roll100(ts->chance_middle_close))
    {
        LOGD("Disturber: Closing connection in the middle of transmission");
        lineReuseBuffer(l, buf);
        tunnelPrevDownStreamFinish(t, l);
        return;
    }

    if (roll100(ts->chance_payload_loss))
    {
        LOGD("Disturber: Dropping payload (chance: %d%%)", ts->chance_payload_loss);
        lineReuseBuffer(l, buf);
        return;
    }

    if (roll100(ts->chance_payload_duplication))
    {
        LOGD("Disturber: Duplicating payload (chance: %d%%)", ts->chance_payload_duplication);
        sbuf_t *dup_buf = sbufDuplicate(buf);


        if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, dup_buf))
        {
            // Line may already be destroyed here, so avoid line-bound reuse API.
            reuseBuffer(buf);
            return;
        }
    }

    if (roll100(ts->chance_payload_corruption))
    {
        uint8_t *data = sbufGetMutablePtr(buf);
        uint32_t size = sbufGetLength(buf);

        if (size > 0)
        {
            // Corrupt up to 10% of the payload, at least 1 byte
            uint32_t corrupt_bytes = (size > 10) ? (size / 10) : 1;
            for (uint32_t i = 0; i < corrupt_bytes; i++)
            {
                uint32_t offset = fastRand() % size;
                data[offset] ^= (uint8_t) (fastRand() & 0xFF);
            }
            LOGD("Disturber: Corrupted payload (corrupted bytes: %u)", corrupt_bytes);
        }
    }

    if (ls->held_payload != NULL)
    {
        LOGD("Disturber: Sending held payload before current one (chance: %d%%)", ts->chance_payload_out_of_order);

        sbuf_t *held_buf = ls->held_payload;
        ls->held_payload = NULL;

        if (! withLineLockedWithBuf(l,tunnelNextUpStreamPayload, t, held_buf))
        {
            // Line may already be destroyed here, so avoid line-bound reuse API.
            reuseBuffer(buf);
            return;
        }

        tunnelNextUpStreamPayload(t, l, buf);
        return;
    }
    else
    {
        if (roll100(ts->chance_payload_out_of_order))
        {
            ls->held_payload = buf;
            return;
        }
    }

    if (roll100(ts->chance_payload_delay))
    {
        int delay_range = ts->delay_max_ms - ts->delay_min_ms + 1;
        if (delay_range <= 0)
        {
            // Fallback safety for malformed runtime config.
            delay_range = 1;
        }
        int delay_ms = ts->delay_min_ms + ((int) fastRand() % delay_range);
        LOGD("Disturber: Delaying payload by %d ms (chance: %d%%)", delay_ms, ts->chance_payload_delay);
        lineLock(l);
        lineScheduleDelayedTaskWithBuf(l, disturberTunnelUpStreamPayload, delay_ms, t,  buf);
        lineUnlock(l);
        return;
    }


    if(roll100(ts->chance_connection_deadhang))
    {
        LOGD("Disturber: Putting connection into deadhang (chance: %d%%)", ts->chance_connection_deadhang);
        ls->is_deadhang = true;
        lineReuseBuffer(l, buf);
        return;
    }

    tunnelNextUpStreamPayload(t, l, buf);
}
