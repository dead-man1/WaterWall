#include "structure.h"

#include "loggers/network_logger.h"

enum
{
    kIpv4MinHeaderLength = 20,
    kIpv4FragmentMask    = 0x3FFF
};

static bool pingserverValidateIpv4PacketBytes(const uint8_t *packet, uint32_t available_len, uint16_t *packet_len_out)
{
    if (available_len < sizeof(struct ip_hdr))
    {
        return false;
    }

    const struct ip_hdr *ipheader         = (const struct ip_hdr *) packet;
    const uint16_t       ip_header_len    = IPH_HL_BYTES(ipheader);
    const uint16_t       total_packet_len = lwip_ntohs(IPH_LEN(ipheader));

    if (IPH_V(ipheader) != 4)
    {
        return false;
    }

    if (ip_header_len < kIpv4MinHeaderLength || total_packet_len < ip_header_len || total_packet_len != available_len)
    {
        return false;
    }

    *packet_len_out = total_packet_len;
    return true;
}

static bool pingserverPrepareInnerPacket(line_t *l, sbuf_t *buf, uint16_t *inner_packet_len_out)
{
    if (! pingserverValidateIpv4PacketBytes(sbufGetRawPtr(buf), sbufGetLength(buf), inner_packet_len_out))
    {
        return false;
    }

    if (lineGetRecalculateChecksum(l))
    {
        calcFullPacketChecksum(sbufGetMutablePtr(buf));
        lineSetRecalculateChecksum(l, false);
    }

    return true;
}

static bool pingserverMatchEnvelope(const pingserver_tstate_t *state, sbuf_t *buf, uint16_t *outer_header_len_out)
{
    if (sbufGetLength(buf) < kPingServerEncapsulationOverhead)
    {
        return false;
    }

    const uint8_t       *packet   = sbufGetRawPtr(buf);
    const struct ip_hdr *ipheader = (const struct ip_hdr *) packet;

    if (IPH_V(ipheader) != 4)
    {
        return false;
    }

    const uint16_t ip_header_len = IPH_HL_BYTES(ipheader);
    if (ip_header_len < kIpv4MinHeaderLength)
    {
        return false;
    }

    if (sbufGetLength(buf) < (uint32_t) ip_header_len + kPingServerIcmpHeaderLength)
    {
        return false;
    }

    const uint16_t total_packet_len = lwip_ntohs(IPH_LEN(ipheader));
    if (total_packet_len != sbufGetLength(buf))
    {
        return false;
    }

    if (IPH_PROTO(ipheader) != IP_PROTO_ICMP)
    {
        return false;
    }

    if ((lwip_ntohs(IPH_OFFSET(ipheader)) & kIpv4FragmentMask) != 0)
    {
        return false;
    }

    if (ipheader->src.addr != state->peer_ipv4 || ipheader->dest.addr != state->local_ipv4)
    {
        return false;
    }

    const struct icmp_echo_hdr *icmpheader = (const struct icmp_echo_hdr *) (packet + ip_header_len);
    if (icmpheader->type != ICMP_ECHO || icmpheader->code != 0 || icmpheader->id != lwip_htons(state->identifier))
    {
        return false;
    }

    *outer_header_len_out = (uint16_t) (ip_header_len + kPingServerIcmpHeaderLength);
    return true;
}

void pingserverEncapsulatePacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingserver_tstate_t *state = tunnelGetState(t);
    uint16_t             inner_packet_len;

    if (! pingserverPrepareInnerPacket(l, buf, &inner_packet_len))
    {
        lineReuseBuffer(l, buf);
        return;
    }

    if (inner_packet_len > kPingServerMaxInnerPacketLength)
    {
        LOGW("PingServer: inner IPv4 packet exceeds ICMP tunnel size limit: %u > %u", inner_packet_len,
             (unsigned int) kPingServerMaxInnerPacketLength);
        lineReuseBuffer(l, buf);
        return;
    }

    if (sbufGetLeftCapacity(buf) < kPingServerEncapsulationOverhead)
    {
        LOGW("PingServer: dropping packet without enough left padding for IPv4+ICMP encapsulation");
        lineReuseBuffer(l, buf);
        return;
    }

    sbufShiftLeft(buf, kPingServerEncapsulationOverhead);

    uint8_t   *packet     = sbufGetMutablePtr(buf);
    struct ip_hdr        *ipheader   = (struct ip_hdr *) packet;
    struct icmp_echo_hdr *icmpheader = (struct icmp_echo_hdr *) (packet + kPingServerIpv4HeaderLength);

    memorySet(packet, 0, kPingServerEncapsulationOverhead);

    IPH_VHL_SET(ipheader, 4, sizeof(struct ip_hdr) / 4U);
    IPH_TOS_SET(ipheader, state->tos);
    IPH_LEN_SET(ipheader, lwip_htons((uint16_t) (inner_packet_len + kPingServerEncapsulationOverhead)));
    IPH_ID_SET(ipheader, lwip_htons((uint16_t) (atomicAdd(&(state->ipv4_identification), 1U) + 1U)));
    IPH_OFFSET_SET(ipheader, 0);
    IPH_TTL_SET(ipheader, state->ttl);
    IPH_PROTO_SET(ipheader, IP_PROTO_ICMP);
    IPH_CHKSUM_SET(ipheader, 0);
    ipheader->src.addr  = state->local_ipv4;
    ipheader->dest.addr = state->peer_ipv4;

    ICMPH_TYPE_SET(icmpheader, ICMP_ECHO);
    ICMPH_CODE_SET(icmpheader, 0);
    icmpheader->chksum = 0;
    icmpheader->id     = lwip_htons(state->identifier);
    icmpheader->seqno  = lwip_htons((uint16_t) (atomicAdd(&(state->icmp_sequence), 1U) + 1U));

    calcFullPacketChecksum(packet);
    lineSetRecalculateChecksum(l, false);

    tunnelPrevDownStreamPayload(t, l, buf);
}

void pingserverDecapsulatePacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingserver_tstate_t *state = tunnelGetState(t);
    uint16_t             outer_header_len;

    if (! pingserverMatchEnvelope(state, buf, &outer_header_len))
    {
        tunnelNextUpStreamPayload(t, l, buf);
        return;
    }

    const uint32_t inner_packet_len = sbufGetLength(buf) - outer_header_len;
    uint16_t       validated_inner_len;

    if (! pingserverValidateIpv4PacketBytes((const uint8_t *) sbufGetRawPtr(buf) + outer_header_len, inner_packet_len,
                                            &validated_inner_len))
    {
        tunnelNextUpStreamPayload(t, l, buf);
        return;
    }

    sbufShiftRight(buf, outer_header_len);
    sbufSetLength(buf, validated_inner_len);
    lineSetRecalculateChecksum(l, false);

    tunnelNextUpStreamPayload(t, l, buf);
}
