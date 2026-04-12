#include "structure.h"

void packetsplitstreamTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    packetsplitstream_lstate_t *child_ls = lineGetState(l, t);

    if (child_ls->role == kPacketSplitStreamRoleDownload && child_ls->packet_line != NULL)
    {
        tunnelPrevDownStreamPayload(t, child_ls->packet_line, buf);
        return;
    }

    lineReuseBuffer(l, buf);
}
