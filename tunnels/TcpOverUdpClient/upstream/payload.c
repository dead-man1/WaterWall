#include "structure.h"

#include "loggers/network_logger.h"

static void pauseDownSide(tunnel_t *t, line_t *l)
{
    tcpoverudpclient_lstate_t *ls = lineGetState(l, t);

    if (ls->can_downstream)
    {
        tunnelPrevDownStreamPause(t, l);
    }
}

void tcpoverudpclientTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    tcpoverudpclient_lstate_t *ls = lineGetState(l, t);

    if (UNLIKELY(ls->k_handle == NULL))
    {
        lineReuseBuffer(l, buf);
        return;
    }

    if (ikcp_waitsnd(ls->k_handle) > KCP_SEND_WINDOW_LIMIT)
    {
        ls->write_paused = true;
        lineScheduleTask(l, pauseDownSide, t);
    }

    // Break buffer into chunks of less than 4096 bytes and send in order

    while (sbufGetLength(buf) > 0)
    {
        int write_size = min(KCP_MTU_WRITE, sbufGetLength(buf));

        sbufShiftLeft(buf, kFrameHeaderLength);
        sbufWriteUI8(buf, kFrameFlagData);

        ikcp_send(ls->k_handle, (void *) sbufGetMutablePtr(buf), write_size + kFrameHeaderLength);
        sbufShiftRight(buf, write_size + kFrameHeaderLength);
    }
    lineReuseBuffer(l, buf);

    // Update KCP state after sending to trigger immediate transmission
    tcpoverudpclientUpdateKcp(ls, false);
}
