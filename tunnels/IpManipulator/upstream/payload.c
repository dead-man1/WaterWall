#include "structure.h"

#include "loggers/network_logger.h"

#include "tricks/protoswap/trick.h"
#include "tricks/sniblender/trick.h"
#include "tricks/tcpbitchange/trick.h"

void ipmanipulatorUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);

    if (state->trick_proto_swap)
    {
        protoswaptrickUpStreamPayload(t, l, buf);
    }

    if (state->trick_tcp_bit_changes)
    {
        tcpbitchangetrickUpStreamPayload(t, l, buf);
    }

    if (state->trick_sni_blender && sniblendertrickUpStreamPayload(t, l, buf))
    {
        return;
    }

    tunnelNextUpStreamPayload(t, l, buf);
}
