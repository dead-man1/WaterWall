#include "structure.h"

void packetsplitstreamTunnelUpStreamFinish(tunnel_t *t, line_t *l)
{
    packetsplitstream_lstate_t *ls = lineGetState(l, t);

    if (ls->role != kPacketSplitStreamRolePacket)
    {
        return;
    }

    packetsplitstreamDestroyOwnedLine(t, &ls->upload_line);
    packetsplitstreamDestroyOwnedLine(t, &ls->download_line);
    packetsplitstreamLinestateDestroy(ls);
}
