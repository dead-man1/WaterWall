#include "trick.h"

#include "loggers/network_logger.h"

static void protoswapApply(tunnel_t *t, line_t *l, sbuf_t *buf, bool upstream)
{
    ipmanipulator_tstate_t *state    = tunnelGetState(t);
    struct ip_hdr          *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (IPH_V(ipheader) != 4)
    {
        return;
    }

    if (state->trick_proto_swap_tcp_number != -1)
    {
        if (IPH_PROTO(ipheader) == IPPROTO_TCP)
        {
            if (state->trick_proto_swap_tcp_number_2 != -1)
            {
                int *toggle = upstream ? &state->trick_proto_swap_tcp_toggle_up : &state->trick_proto_swap_tcp_toggle_down;
                IPH_PROTO_SET(ipheader,
                              (*toggle == 0) ? state->trick_proto_swap_tcp_number : state->trick_proto_swap_tcp_number_2);
                *toggle = (*toggle == 0) ? 1 : 0;
            }
            else
            {
                IPH_PROTO_SET(ipheader, state->trick_proto_swap_tcp_number);
            }
            l->recalculate_checksum = true;
        }
        else if (IPH_PROTO(ipheader) == state->trick_proto_swap_tcp_number ||
                 IPH_PROTO(ipheader) == state->trick_proto_swap_tcp_number_2)
        {
            IPH_PROTO_SET(ipheader, IPPROTO_TCP);
            l->recalculate_checksum = true;
        }
    }

    if (state->trick_proto_swap_udp_number != -1)
    {
        if (IPH_PROTO(ipheader) == IPPROTO_UDP)
        {
            IPH_PROTO_SET(ipheader, state->trick_proto_swap_udp_number);
            l->recalculate_checksum = true;
        }
        else if (IPH_PROTO(ipheader) == state->trick_proto_swap_udp_number)
        {
            IPH_PROTO_SET(ipheader, IPPROTO_UDP);
            l->recalculate_checksum = true;
        }
    }
}

void protoswaptrickUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    protoswapApply(t, l, buf, true);
}

void protoswaptrickDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    protoswapApply(t, l, buf, false);
}
