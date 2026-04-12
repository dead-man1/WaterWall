#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelDestroy(tunnel_t *t)
{
    tunnel_chain_t *chain = tunnelGetChain(t);

    if (chain && chain->packet_lines)
    {
        for (wid_t wi = 0; wi < chain->workers_count; ++wi)
        {
            line_t                  *packet_line = tunnelchainGetWorkerPacketLine(chain, wi);
            packetstostream_lstate_t *ls         = lineGetState(packet_line, t);

       

            if (ls->read_stream.pool != NULL)
            {
                packetstostreamLinestateDestroy(ls);
            }
        }
    }

    tunnelDestroy(t);
}
