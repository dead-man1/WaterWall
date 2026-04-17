#include "structure.h"

#include "loggers/network_logger.h"

void muxclientTunnelUpStreamInit(tunnel_t *t, line_t *child_l)
{
    muxclient_tstate_t *ts       = tunnelGetState(t);
    muxclient_lstate_t *child_ls = lineGetState(child_l, t);
    wid_t               wid      = lineGetWID(child_l);

    if (ts->unsatisfied_lines[wid] == NULL ||
        muxclientCheckConnectionIsExhausted(ts, lineGetState(ts->unsatisfied_lines[wid], t)))
    {
        line_t             *parent_l  = lineCreate(tunnelchainGetLinePools(tunnelGetChain(t)), wid);
        muxclient_lstate_t *parent_ls = lineGetState(parent_l, t);

        muxclientLinestateInitialize(parent_ls, parent_l, false, 0);

        if (! withLineLocked(parent_l, tunnelNextUpStreamInit, t))
        {
            tunnelPrevDownStreamFinish(t, child_l);
            return;
        }

        ts->unsatisfied_lines[wid] = parent_l;
    }

    line_t             *parent_l  = ts->unsatisfied_lines[wid];
    muxclient_lstate_t *parent_ls = lineGetState(parent_l, t);
    assert(parent_ls->connection_id < CID_MAX);

    cid_t new_cid = parent_ls->connection_id + 1;

    muxclientLinestateInitialize(child_ls, child_l, true, new_cid);
    muxclientJoinConnection(parent_ls, child_ls);

    sbuf_t *initpacket_buf = bufferpoolGetLargeBuffer(lineGetBufferPool(parent_l));
    muxclientMakeMuxFrame(initpacket_buf, new_cid, kMuxFlagOpen);

    if (! withLineLockedWithBuf(parent_l, tunnelNextUpStreamPayload, t, initpacket_buf))
    {
        return;
    }

    parent_ls->connection_id = new_cid;
    tunnelPrevDownStreamEst(t, child_l);
}
