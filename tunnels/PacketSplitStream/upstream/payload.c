#include "structure.h"

void packetsplitstreamTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    packetsplitstream_lstate_t *ls = lineGetState(l, t);
    packetsplitstream_tstate_t *ts = tunnelGetState(t);
    line_t                     *upload_line;

    if (ls->role != kPacketSplitStreamRolePacket)
    {
        packetsplitstreamLinestateInitializePacket(ls, l);
    }

    upload_line = packetsplitstreamEnsureUploadLine(t, l, ls);
    if (upload_line == NULL)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    tunnelUpStreamPayload(ts->up_tunnel, upload_line, buf);
}
