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

static void pingserverXorPayload(uint8_t *payload, uint16_t payload_len, uint8_t xor_byte)
{
    for (uint16_t i = 0; i < payload_len; ++i)
    {
        payload[i] ^= xor_byte;
    }
}

static bool pingserverChooseRoundupPayloadLength(uint16_t payload_with_size_len, uint16_t *roundup_payload_len_out)
{
    static const uint16_t kRoundupBuckets[] = {64, 128, 256, 512, 1024, kPingServerMaxIcmpPayloadLength};

    for (size_t i = 0; i < ARRAY_SIZE(kRoundupBuckets); ++i)
    {
        if (payload_with_size_len <= kRoundupBuckets[i])
        {
            *roundup_payload_len_out = kRoundupBuckets[i];
            return true;
        }
    }

    return false;
}

static sbuf_t *pingserverPreparePayloadBuffer(tunnel_t *t, line_t *l, sbuf_t *buf, uint16_t *icmp_payload_len_out)
{
    pingserver_tstate_t *state = tunnelGetState(t);
    uint16_t             inner_packet_len;

    if (! pingserverValidateIpv4PacketBytes(sbufGetRawPtr(buf), sbufGetLength(buf), &inner_packet_len))
    {
        return NULL;
    }

    if (lineGetRecalculateChecksum(l))
    {
        calcFullPacketChecksum(sbufGetMutablePtr(buf));
        lineSetRecalculateChecksum(l, false);
    }

    uint16_t icmp_payload_len = inner_packet_len;

    if (state->roundup_payload_size)
    {
        if (inner_packet_len > kPingServerMaxIcmpPayloadLength - kPingServerSizePrefixLength)
        {
            LOGW("PingServer: dropping packet because roundup-size needs %u bytes but ICMP payload is capped at %u",
                 (unsigned int) (inner_packet_len + kPingServerSizePrefixLength),
                 (unsigned int) kPingServerMaxIcmpPayloadLength);
            return NULL;
        }

        const uint16_t payload_with_size_len = (uint16_t) (inner_packet_len + kPingServerSizePrefixLength);
        if (! pingserverChooseRoundupPayloadLength(payload_with_size_len, &icmp_payload_len))
        {
            LOGW("PingServer: dropping packet because roundup-size cannot fit %u bytes inside the ICMP payload limit",
                 (unsigned int) payload_with_size_len);
            return NULL;
        }

        buf = sbufReserveSpace(buf, icmp_payload_len);

        uint8_t *payload = sbufGetMutablePtr(buf);
        memoryMove(payload + kPingServerSizePrefixLength, payload, inner_packet_len);
        payload[0] = (uint8_t) ((inner_packet_len >> 8) & 0xFFU);
        payload[1] = (uint8_t) (inner_packet_len & 0xFFU);

        if (icmp_payload_len > payload_with_size_len)
        {
            getRandomBytes(payload + payload_with_size_len, (size_t) (icmp_payload_len - payload_with_size_len));
        }

        sbufSetLength(buf, icmp_payload_len);
    }
    else if (inner_packet_len > kPingServerMaxIcmpPayloadLength)
    {
        LOGW("PingServer: inner IPv4 packet exceeds ICMP payload size limit: %u > %u", inner_packet_len,
             (unsigned int) kPingServerMaxIcmpPayloadLength);
        return NULL;
    }

    if (state->payload_xor_enabled)
    {
        pingserverXorPayload(sbufGetMutablePtr(buf), icmp_payload_len, state->payload_xor_byte);
    }

    *icmp_payload_len_out = icmp_payload_len;
    return buf;
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

    if (total_packet_len > kPingServerNetworkMtu)
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
    uint16_t             icmp_payload_len;

    sbuf_t *prepared_buf = pingserverPreparePayloadBuffer(t, l, buf, &icmp_payload_len);
    if (prepared_buf == NULL)
    {
        lineReuseBuffer(l, buf);
        return;
    }
    buf = prepared_buf;

    const struct ip_hdr *inner_ipheader = (const struct ip_hdr *) sbufGetRawPtr(buf);
    const uint32_t       outer_src_addr = inner_ipheader->src.addr;
    const uint32_t       outer_dest_addr = inner_ipheader->dest.addr;

    if (sbufGetLeftCapacity(buf) < kPingServerEncapsulationOverhead)
    {
        LOGW("PingServer: dropping packet without enough left padding for IPv4+ICMP encapsulation");
        lineReuseBuffer(l, buf);
        return;
    }

    sbufShiftLeft(buf, kPingServerEncapsulationOverhead);

    uint8_t              *packet     = sbufGetMutablePtr(buf);
    struct ip_hdr        *ipheader   = (struct ip_hdr *) packet;
    struct icmp_echo_hdr *icmpheader = (struct icmp_echo_hdr *) (packet + kPingServerIpv4HeaderLength);

    memorySet(packet, 0, kPingServerEncapsulationOverhead);

    IPH_VHL_SET(ipheader, 4, sizeof(struct ip_hdr) / 4U);
    IPH_TOS_SET(ipheader, state->tos);
    IPH_LEN_SET(ipheader, lwip_htons((uint16_t) (icmp_payload_len + kPingServerEncapsulationOverhead)));
    IPH_ID_SET(ipheader, lwip_htons((uint16_t) (atomicAdd(&(state->ipv4_identification), 1U) + 1U)));
    IPH_OFFSET_SET(ipheader, 0);
    IPH_TTL_SET(ipheader, state->ttl);
    IPH_PROTO_SET(ipheader, IP_PROTO_ICMP);
    IPH_CHKSUM_SET(ipheader, 0);
    ipheader->src.addr  = outer_src_addr;
    ipheader->dest.addr = outer_dest_addr;

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

    uint8_t *icmp_payload             = sbufGetMutablePtr(buf) + outer_header_len;
    uint16_t icmp_payload_len         = (uint16_t) (sbufGetLength(buf) - outer_header_len);
    bool     payload_decoded_in_place = false;

    if (state->payload_xor_enabled)
    {
        pingserverXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
        payload_decoded_in_place = true;
    }

    uint8_t  *inner_packet_ptr = icmp_payload;
    uint16_t  inner_packet_len = icmp_payload_len;

    if (state->roundup_payload_size)
    {
        if (icmp_payload_len < kPingServerSizePrefixLength)
        {
            if (payload_decoded_in_place)
            {
                pingserverXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
            }
            tunnelNextUpStreamPayload(t, l, buf);
            return;
        }

        inner_packet_len = (uint16_t) ((((uint16_t) icmp_payload[0]) << 8) | icmp_payload[1]);
        if (inner_packet_len > icmp_payload_len - kPingServerSizePrefixLength)
        {
            if (payload_decoded_in_place)
            {
                pingserverXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
            }
            tunnelNextUpStreamPayload(t, l, buf);
            return;
        }

        inner_packet_ptr = icmp_payload + kPingServerSizePrefixLength;
    }

    uint16_t validated_inner_len;
    if (! pingserverValidateIpv4PacketBytes(inner_packet_ptr, inner_packet_len, &validated_inner_len))
    {
        if (payload_decoded_in_place)
        {
            pingserverXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
        }
        tunnelNextUpStreamPayload(t, l, buf);
        return;
    }

    sbufShiftRight(buf, outer_header_len);

    if (state->roundup_payload_size)
    {
        memoryMove(sbufGetMutablePtr(buf), sbufGetMutablePtr(buf) + kPingServerSizePrefixLength, validated_inner_len);
    }

    sbufSetLength(buf, validated_inner_len);
    lineSetRecalculateChecksum(l, false);

    tunnelNextUpStreamPayload(t, l, buf);
}
