#include "structure.h"

#include "loggers/network_logger.h"

enum
{
    kIpv4MinHeaderLength = 20,
    kIpv4FragmentMask    = 0x3FFF
};

static uint8_t pingclientPeekPacketVersion(sbuf_t *buf)
{
    if (sbufGetLength(buf) == 0)
    {
        return 0;
    }

    const uint8_t *packet = (const uint8_t *) sbufGetRawPtr(buf);
    return (uint8_t) (packet[0] >> 4);
}

static void pingclientFormatIpv4(char *dest, size_t dest_len, uint32_t addr)
{
    ip4_addr_t ipaddr;
    ip4AddrSetU32(&ipaddr, addr);
    stringCopyN(dest, ip4AddrNetworkToAddress(&ipaddr), dest_len);
}

static void pingclientLogIpv6Drop(const char *path)
{
    LOGW("PingClient: dropping IPv6 packet on %s path because PingClient only supports IPv4", path);
}

static bool pingclientValidateIpv4PacketBytes(const uint8_t *packet, uint32_t available_len, uint16_t *packet_len_out)
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

static void pingclientXorPayload(uint8_t *payload, uint16_t payload_len, uint8_t xor_byte)
{
    for (uint16_t i = 0; i < payload_len; ++i)
    {
        payload[i] ^= xor_byte;
    }
}

static bool pingclientChooseRoundupPayloadLength(uint16_t payload_with_size_len, uint16_t *roundup_payload_len_out)
{
    static const uint16_t kRoundupBuckets[] = {56, 128, 256, 512, 1024, kPingClientMaxIcmpPayloadLength};

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

static bool pingclientStrategyNeedsIcmpEnvelope(const pingclient_tstate_t *state)
{
    return state->strategy != kPingClientStrategyChangeOnlyIpv4ProtocolNumber;
}

static bool pingclientStrategyNeedsInnerIpv4Validation(const pingclient_tstate_t *state)
{
    return state->strategy != kPingClientStrategyWrapOnlyIcmpHeader;
}

static sbuf_t *pingclientPreparePayloadBuffer(tunnel_t *t, line_t *l, sbuf_t *buf, uint16_t *icmp_payload_len_out)
{
    pingclient_tstate_t *state = tunnelGetState(t);
    uint16_t             inner_packet_len;

    if (! pingclientValidateIpv4PacketBytes(sbufGetRawPtr(buf), sbufGetLength(buf), &inner_packet_len))
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
        if (inner_packet_len > kPingClientMaxIcmpPayloadLength - kPingClientSizePrefixLength)
        {
            LOGW("PingClient: dropping packet because roundup-size needs %u bytes but ICMP payload is capped at %u",
                 (unsigned int) (inner_packet_len + kPingClientSizePrefixLength),
                 (unsigned int) kPingClientMaxIcmpPayloadLength);
            return NULL;
        }

        const uint16_t payload_with_size_len = (uint16_t) (inner_packet_len + kPingClientSizePrefixLength);
        if (! pingclientChooseRoundupPayloadLength(payload_with_size_len, &icmp_payload_len))
        {
            LOGW("PingClient: dropping packet because roundup-size cannot fit %u bytes inside the ICMP payload limit",
                 (unsigned int) payload_with_size_len);
            return NULL;
        }

        buf = sbufReserveSpace(buf, icmp_payload_len);

        uint8_t *payload = sbufGetMutablePtr(buf);
        memoryMove(payload + kPingClientSizePrefixLength, payload, inner_packet_len);
        payload[0] = (uint8_t) ((inner_packet_len >> 8) & 0xFFU);
        payload[1] = (uint8_t) (inner_packet_len & 0xFFU);

        if (icmp_payload_len > payload_with_size_len)
        {
            getRandomBytes(payload + payload_with_size_len, (size_t) (icmp_payload_len - payload_with_size_len));
        }

        sbufSetLength(buf, icmp_payload_len);
    }
    else if (inner_packet_len > kPingClientMaxIcmpPayloadLength)
    {
        LOGW("PingClient: inner IPv4 packet exceeds ICMP payload size limit: %u > %u", inner_packet_len,
             (unsigned int) kPingClientMaxIcmpPayloadLength);
        return NULL;
    }

    if (state->payload_xor_enabled)
    {
        pingclientXorPayload(sbufGetMutablePtr(buf), icmp_payload_len, state->payload_xor_byte);
    }

    *icmp_payload_len_out = icmp_payload_len;
    return buf;
}

static void pingclientLogSourceDestMismatch(const pingclient_tstate_t *state, const struct ip_hdr *ipheader)
{
    char expected_src[40];
    char expected_dest[40];
    char actual_src[40];
    char actual_dest[40];

    pingclientFormatIpv4(expected_src, sizeof(expected_src), state->source_addr);
    pingclientFormatIpv4(expected_dest, sizeof(expected_dest), state->dest_addr);
    pingclientFormatIpv4(actual_src, sizeof(actual_src), ipheader->src.addr);
    pingclientFormatIpv4(actual_dest, sizeof(actual_dest), ipheader->dest.addr);

    LOGW("PingClient: ICMP tunnel packet failed source/dest verification, expected %s -> %s but got %s -> %s",
         expected_src, expected_dest, actual_src, actual_dest);
}

static bool pingclientMatchEnvelope(const pingclient_tstate_t *state, sbuf_t *buf, uint16_t *outer_header_len_out)
{
    if (sbufGetLength(buf) < kPingClientEncapsulationOverhead)
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

    if (sbufGetLength(buf) < (uint32_t) ip_header_len + kPingClientIcmpHeaderLength)
    {
        return false;
    }

    const uint16_t total_packet_len = lwip_ntohs(IPH_LEN(ipheader));
    if (total_packet_len != sbufGetLength(buf))
    {
        return false;
    }

    if (total_packet_len > kPingClientNetworkMtu)
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

    if (state->strategy == kPingClientStrategyWrapNewIpAndIcmpHeader &&
        (ipheader->src.addr != state->source_addr || ipheader->dest.addr != state->dest_addr))
    {
        pingclientLogSourceDestMismatch(state, ipheader);
        return false;
    }

    *outer_header_len_out = (uint16_t) (ip_header_len + kPingClientIcmpHeaderLength);
    return true;
}

static bool pingclientHandleUnmatchedDownstreamPacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    if (pingclientPeekPacketVersion(buf) == 6)
    {
        pingclientLogIpv6Drop("downstream");
        lineReuseBuffer(l, buf);
        return false;
    }

    tunnelPrevDownStreamPayload(t, l, buf);
    return true;
}

static void pingclientEncapsulateWithIcmp(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingclient_tstate_t *state = tunnelGetState(t);
    uint16_t             icmp_payload_len;

    sbuf_t *prepared_buf = pingclientPreparePayloadBuffer(t, l, buf, &icmp_payload_len);
    if (prepared_buf == NULL)
    {
        if (pingclientPeekPacketVersion(buf) == 6)
        {
            pingclientLogIpv6Drop("upstream");
        }
        else
        {
            LOGW("PingClient: dropping upstream packet because encapsulation requires a valid IPv4 packet");
        }

        lineReuseBuffer(l, buf);
        return;
    }
    buf = prepared_buf;

    const struct ip_hdr *inner_ipheader  = (const struct ip_hdr *) sbufGetRawPtr(buf);
    uint32_t             outer_src_addr  = inner_ipheader->src.addr;
    uint32_t             outer_dest_addr = inner_ipheader->dest.addr;
    uint8_t              outer_ttl       = state->ttl;
    uint8_t              outer_tos       = state->tos;

    if (state->strategy == kPingClientStrategyWrapNewIpAndIcmpHeader)
    {
        outer_src_addr  = state->source_addr;
        outer_dest_addr = state->dest_addr;
    }
    if (sbufGetLeftCapacity(buf) < kPingClientEncapsulationOverhead)
    {
        LOGW("PingClient: dropping packet without enough left padding for IPv4+ICMP encapsulation");
        lineReuseBuffer(l, buf);
        return;
    }

    sbufShiftLeft(buf, kPingClientEncapsulationOverhead);

    uint8_t              *packet     = sbufGetMutablePtr(buf);
    struct ip_hdr        *ipheader   = (struct ip_hdr *) packet;
    struct icmp_echo_hdr *icmpheader = (struct icmp_echo_hdr *) (packet + kPingClientIpv4HeaderLength);

    memorySet(packet, 0, kPingClientEncapsulationOverhead);

    IPH_VHL_SET(ipheader, 4, sizeof(struct ip_hdr) / 4U);
    IPH_TOS_SET(ipheader, outer_tos);
    IPH_LEN_SET(ipheader, lwip_htons((uint16_t) (icmp_payload_len + kPingClientEncapsulationOverhead)));
    IPH_ID_SET(ipheader, lwip_htons((uint16_t) (atomicAdd(&(state->ipv4_identification), 1U) + 1U)));
    IPH_OFFSET_SET(ipheader, 0);
    IPH_TTL_SET(ipheader, outer_ttl);
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

    tunnelNextUpStreamPayload(t, l, buf);
}

static void pingclientDecapsulateIcmp(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingclient_tstate_t *state = tunnelGetState(t);
    uint16_t             outer_header_len;

    if (! pingclientMatchEnvelope(state, buf, &outer_header_len))
    {
        pingclientHandleUnmatchedDownstreamPacket(t, l, buf);
        return;
    }

    uint8_t *icmp_payload             = sbufGetMutablePtr(buf) + outer_header_len;
    uint16_t icmp_payload_len         = (uint16_t) (sbufGetLength(buf) - outer_header_len);
    bool     payload_decoded_in_place = false;

    if (state->payload_xor_enabled)
    {
        pingclientXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
        payload_decoded_in_place = true;
    }

    uint8_t  *inner_packet_ptr = icmp_payload;
    uint16_t  inner_packet_len = icmp_payload_len;

    if (state->roundup_payload_size)
    {
        if (icmp_payload_len < kPingClientSizePrefixLength)
        {
            if (payload_decoded_in_place)
            {
                pingclientXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
            }
            tunnelPrevDownStreamPayload(t, l, buf);
            return;
        }

        inner_packet_len = (uint16_t) ((((uint16_t) icmp_payload[0]) << 8) | icmp_payload[1]);
        if (inner_packet_len > icmp_payload_len - kPingClientSizePrefixLength)
        {
            if (payload_decoded_in_place)
            {
                pingclientXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
            }
            tunnelPrevDownStreamPayload(t, l, buf);
            return;
        }

        inner_packet_ptr = icmp_payload + kPingClientSizePrefixLength;
    }

    uint16_t payload_len_after_strip = inner_packet_len;

    if (pingclientStrategyNeedsInnerIpv4Validation(state))
    {
        uint16_t validated_inner_len;
        if (! pingclientValidateIpv4PacketBytes(inner_packet_ptr, inner_packet_len, &validated_inner_len))
        {
            if (payload_decoded_in_place)
            {
                pingclientXorPayload(icmp_payload, icmp_payload_len, state->payload_xor_byte);
            }
            tunnelPrevDownStreamPayload(t, l, buf);
            return;
        }

        payload_len_after_strip = validated_inner_len;
    }

    sbufShiftRight(buf, outer_header_len);

    if (state->roundup_payload_size)
    {
        memoryMove(sbufGetMutablePtr(buf), sbufGetMutablePtr(buf) + kPingClientSizePrefixLength, payload_len_after_strip);
    }

    sbufSetLength(buf, payload_len_after_strip);
    lineSetRecalculateChecksum(l, false);

    tunnelPrevDownStreamPayload(t, l, buf);
}

static void pingclientSwapIpv4ProtocolToIcmp(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingclient_tstate_t *state = tunnelGetState(t);
    uint16_t             packet_len;

    if (! pingclientValidateIpv4PacketBytes(sbufGetRawPtr(buf), sbufGetLength(buf), &packet_len))
    {
        discard packet_len;
        if (pingclientPeekPacketVersion(buf) == 6)
        {
            pingclientLogIpv6Drop("upstream");
        }
        else
        {
            LOGW("PingClient: dropping upstream packet because protocol-swap mode requires a valid IPv4 packet");
        }
        lineReuseBuffer(l, buf);
        return;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);
    if (IPH_PROTO(ipheader) == state->swap_identifier)
    {
        IPH_PROTO_SET(ipheader, IP_PROTO_ICMP);
        lineSetRecalculateChecksum(l, true);
    }

    tunnelNextUpStreamPayload(t, l, buf);
}

static void pingclientRestoreIpv4ProtocolFromIcmp(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingclient_tstate_t *state = tunnelGetState(t);
    uint16_t             packet_len;

    if (! pingclientValidateIpv4PacketBytes(sbufGetRawPtr(buf), sbufGetLength(buf), &packet_len))
    {
        discard packet_len;
        pingclientHandleUnmatchedDownstreamPacket(t, l, buf);
        return;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);
    if (IPH_PROTO(ipheader) == IP_PROTO_ICMP)
    {
        IPH_PROTO_SET(ipheader, state->swap_identifier);
        lineSetRecalculateChecksum(l, true);
    }

    tunnelPrevDownStreamPayload(t, l, buf);
}

void pingclientEncapsulatePacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingclient_tstate_t *state = tunnelGetState(t);

    if (! pingclientStrategyNeedsIcmpEnvelope(state))
    {
        pingclientSwapIpv4ProtocolToIcmp(t, l, buf);
        return;
    }

    pingclientEncapsulateWithIcmp(t, l, buf);
}

void pingclientDecapsulatePacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingclient_tstate_t *state = tunnelGetState(t);

    if (! pingclientStrategyNeedsIcmpEnvelope(state))
    {
        pingclientRestoreIpv4ProtocolFromIcmp(t, l, buf);
        return;
    }

    pingclientDecapsulateIcmp(t, l, buf);
}
