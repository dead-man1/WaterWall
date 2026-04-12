#include "structure.h"

void packetsplitstreamTunnelDownStreamFinish(tunnel_t *t, line_t *l)
{
    packetsplitstream_lstate_t *child_ls   = lineGetState(l, t);
    line_t                     *packet_line = child_ls->packet_line;
    packetsplitstream_role_t    role        = child_ls->role;

    packetsplitstreamLinestateDestroy(child_ls);

    if (lineIsAlive(l))
    {
        lineDestroy(l);
    }

    if (packet_line == NULL)
    {
        return;
    }

    packetsplitstream_lstate_t *packet_ls = lineGetState(packet_line, t);

    if (role == kPacketSplitStreamRoleUpload)
    {
        if (packet_ls->upload_line == l)
        {
            packet_ls->upload_line = NULL;
            (void) packetsplitstreamEnsureUploadLine(t, packet_line, packet_ls);
        }
        return;
    }

    if (role == kPacketSplitStreamRoleDownload)
    {
        if (packet_ls->download_line == l)
        {
            packet_ls->download_line = NULL;
            (void) packetsplitstreamEnsureDownloadLine(t, packet_line, packet_ls);
        }
    }
}
