#include "structure.h"

#include "loggers/network_logger.h"

static line_t **packetsplitstreamSelectSlot(packetsplitstream_lstate_t *packet_ls, packetsplitstream_role_t role)
{
    if (role == kPacketSplitStreamRoleUpload)
    {
        return &packet_ls->upload_line;
    }

    assert(role == kPacketSplitStreamRoleDownload);
    return &packet_ls->download_line;
}

static tunnel_t *packetsplitstreamGetTargetTunnel(tunnel_t *t, packetsplitstream_role_t role)
{
    packetsplitstream_tstate_t *ts = tunnelGetState(t);

    if (role == kPacketSplitStreamRoleUpload)
    {
        return ts->up_tunnel;
    }

    assert(role == kPacketSplitStreamRoleDownload);
    return ts->down_tunnel;
}

static line_t *packetsplitstreamCreateSplitLine(tunnel_t *t, line_t *packet_line, packetsplitstream_lstate_t *packet_ls,
                                                packetsplitstream_role_t role)
{
    line_t  **slot        = packetsplitstreamSelectSlot(packet_ls, role);
    tunnel_t *target      = packetsplitstreamGetTargetTunnel(t, role);
    line_t   *split_line  = lineCreate(tunnelchainGetLinePools(tunnelGetChain(t)), lineGetWID(packet_line));
    packetsplitstream_lstate_t *split_ls = lineGetState(split_line, t);

    packetsplitstreamLinestateInitializeChild(split_ls, packet_line, role);
    *slot = split_line;

    if (! withLineLocked(split_line, tunnelUpStreamInit, target))
    {
        if (*slot == split_line)
        {
            *slot = NULL;
        }
        return *slot;
    }

    return *slot;
}

line_t *packetsplitstreamEnsureUploadLine(tunnel_t *t, line_t *packet_line, packetsplitstream_lstate_t *packet_ls)
{
    if (packet_ls->upload_line != NULL && lineIsAlive(packet_ls->upload_line))
    {
        return packet_ls->upload_line;
    }

    packet_ls->upload_line = NULL;
    return packetsplitstreamCreateSplitLine(t, packet_line, packet_ls, kPacketSplitStreamRoleUpload);
}

line_t *packetsplitstreamEnsureDownloadLine(tunnel_t *t, line_t *packet_line, packetsplitstream_lstate_t *packet_ls)
{
    if (packet_ls->download_line != NULL && lineIsAlive(packet_ls->download_line))
    {
        return packet_ls->download_line;
    }

    packet_ls->download_line = NULL;
    return packetsplitstreamCreateSplitLine(t, packet_line, packet_ls, kPacketSplitStreamRoleDownload);
}

void packetsplitstreamEnsureSplitLines(tunnel_t *t, line_t *packet_line, packetsplitstream_lstate_t *packet_ls)
{
    if (packet_ls->role != kPacketSplitStreamRolePacket)
    {
        packetsplitstreamLinestateInitializePacket(packet_ls, packet_line);
    }

    (void) packetsplitstreamEnsureUploadLine(t, packet_line, packet_ls);
    (void) packetsplitstreamEnsureDownloadLine(t, packet_line, packet_ls);
}

void packetsplitstreamDestroyOwnedLine(tunnel_t *t, line_t **slot)
{
    line_t *line = *slot;

    if (line == NULL)
    {
        return;
    }

    packetsplitstreamLinestateDestroy(lineGetState(line, t));
    *slot = NULL;

    if (lineIsAlive(line))
    {
        lineDestroy(line);
    }
}
