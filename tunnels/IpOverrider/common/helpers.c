#include "structure.h"

#include "loggers/network_logger.h"

static void something(void)
{
    // This function is not implemented yet
}

void ipoverriderReplacerDestModeUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{

    ipoverrider_tstate_t *state = tunnelGetState(t);

    if (state->skip_chance != -1 && (fastRand32() % 100) < (uint32_t) state->skip_chance)
    {
        goto skip;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (state->support4 && IPH_V(ipheader) == 4)
    {
        uint16_t size = lwip_ntohs(IPH_LEN(ipheader));

        if (state->only120 && size > 120)
        {
            goto skip;
        }

        memoryCopy(&(ipheader->dest.addr), &state->ov_4, 4);
        l->recalculate_checksum = true;
    }
    // else if (state->support6 && IPH_V(ipheader) == 6)
    // {
    //     struct ip6_hdr *ip6header = (struct ip6_hdr *) sbufGetMutablePtr(buf);
    //     // alignment assumed to be correct
    //     memoryCopy(&(ip6header->dest.addr), &state->ov_6, 16);
    // }

skip:
    tunnelNextUpStreamPayload(t, l, buf);
}

void ipoverriderReplacerSrcModeUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{

    ipoverrider_tstate_t *state = tunnelGetState(t);

    if (state->skip_chance != -1 && (fastRand32() % 100) < (uint32_t) state->skip_chance)
    {
        goto skip;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (state->support4 && IPH_V(ipheader) == 4)
    {
        uint16_t size = lwip_ntohs(IPH_LEN(ipheader));

        if (state->only120 && size > 120)
        {
            goto skip;
        }

        memoryCopy(&(ipheader->src.addr), &state->ov_4, 4);
        l->recalculate_checksum = true;
    }
    // else if (state->support6 && IPH_V(ipheader) == 6)
    // {
    //     struct ip6_hdr *ip6header = (struct ip6_hdr *) sbufGetMutablePtr(buf);
    //     // alignment assumed to be correct
    //     memoryCopy(&(ip6header->dest.addr), &state->ov_6, 16);
    // }

skip:

    tunnelNextUpStreamPayload(t, l, buf);
}

void ipoverriderReplacerDestModeDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{

    ipoverrider_tstate_t *state = tunnelGetState(t);

    if (state->skip_chance != -1 && (fastRand32() % 100) < (uint32_t) state->skip_chance)
    {
        goto skip;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (state->support4 && IPH_V(ipheader) == 4)
    {
        uint16_t size = lwip_ntohs(IPH_LEN(ipheader));

        if (state->only120 && size > 120)
        {
            goto skip;
        }
        memoryCopy(&(ipheader->dest.addr), &state->ov_4, 4);
        l->recalculate_checksum = true;
    }
    // else if (state->support6 && IPH_V(ipheader) == 6)
    // {
    //     struct ip6_hdr *ip6header = (struct ip6_hdr *) sbufGetMutablePtr(buf);
    //     // alignment assumed to be correct
    //     memoryCopy(&(ip6header->dest.addr), &state->ov_6, 16);
    // }

skip:
    tunnelPrevDownStreamPayload(t, l, buf);
}

void ipoverriderReplacerSrcModeDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{

    ipoverrider_tstate_t *state = tunnelGetState(t);

    if (state->skip_chance != -1 && (fastRand32() % 100) < (uint32_t) state->skip_chance)
    {
        goto skip;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (state->support4 && IPH_V(ipheader) == 4)
    {

        uint16_t size = lwip_ntohs(IPH_LEN(ipheader));

        if (state->only120 && size > 120)
        {
            goto skip;
        }
        memoryCopy(&(ipheader->src.addr), &state->ov_4, 4);
        l->recalculate_checksum = true;
    }
    // else if (state->support6 && IPH_V(ipheader) == 6)
    // {
    //     struct ip6_hdr *ip6header = (struct ip6_hdr *) sbufGetMutablePtr(buf);
    //     // alignment assumed to be correct
    //     memoryCopy(&(ip6header->dest.addr), &state->ov_6, 16);
    // }

skip:

    tunnelPrevDownStreamPayload(t, l, buf);
}
