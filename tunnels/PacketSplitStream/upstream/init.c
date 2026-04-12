#include "structure.h"

void packetsplitstreamTunnelUpStreamInit(tunnel_t *t, line_t *l)
{
    packetsplitstream_lstate_t *ls = lineGetState(l, t);

    packetsplitstreamEnsureSplitLines(t, l, ls);
}
