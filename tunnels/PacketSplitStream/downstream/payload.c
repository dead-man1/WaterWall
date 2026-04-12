#include "structure.h"

void packetsplitstreamTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{

    tunnelPrevDownStreamPayload(t, l, buf);
}
