#include "structure.h"

#include "loggers/network_logger.h"

void muxserverTunnelDownStreamPause(tunnel_t *t, line_t *child_l)
{
    muxserver_lstate_t *child_ls = lineGetState(child_l, t);

    assert(child_ls->is_child);

    sbuf_t *pausepacket_buf = bufferpoolGetLargeBuffer(lineGetBufferPool(child_l));
    muxserverMakeMuxFrame(pausepacket_buf, child_ls->connection_id, kMuxFlagFlowPause);

    line_t             *parent_line = child_ls->parent->l;
    muxserver_lstate_t *parent_ls   = lineGetState(parent_line, t);

    parent_ls->last_writer = child_l; // update the last writer to the current child


    if (! withLineLockedWithBuf(parent_line, tunnelPrevDownStreamPayload, t, pausepacket_buf))
    {
        return;
    }
    parent_ls->last_writer = NULL; // reset the last writer after sending the payload
    // parent_ls->paused      = true;

    // tunnelPrevDownStreamPause(t, parent_line);
}
