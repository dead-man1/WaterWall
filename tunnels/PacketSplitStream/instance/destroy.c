#include "structure.h"

void packetsplitstreamTunnelDestroy(tunnel_t *t)
{
    tunnel_chain_t *chain = tunnelGetChain(t);

    if (chain && chain->packet_lines)
    {
        for (wid_t wi = 0; wi < chain->workers_count; ++wi)
        {
            line_t *packet_line = tunnelchainGetWorkerPacketLine(chain, wi);
            packetsplitstream_lstate_t *packet_ls = lineGetState(packet_line, t);

            if (packet_ls->role != kPacketSplitStreamRolePacket)
            {
                continue;
            }

            packetsplitstreamDestroyOwnedLine(t, &packet_ls->upload_line);
            packetsplitstreamDestroyOwnedLine(t, &packet_ls->download_line);
            packetsplitstreamLinestateDestroy(packet_ls);
        }
    }

    tunnelDestroy(t);
}
