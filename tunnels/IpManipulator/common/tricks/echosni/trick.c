#include "trick.h"

#include "loggers/network_logger.h"

typedef struct echosni_match_s
{
    uint16_t ip_total_len;
    uint16_t tls_record_len;
    uint32_t client_hello_len;
    uint16_t server_name_list_len;
    uint16_t server_name_ext_len;
    uint16_t sni_name_len;

    uint32_t sni_name_offset;
    uint32_t sni_name_len_field_offset;
    uint32_t server_name_list_len_field_offset;
    uint32_t server_name_ext_len_field_offset;
    uint32_t tls_record_len_field_offset;
    uint32_t client_hello_len_field_offset;
} echosni_match_t;

static uint16_t readBe16(const uint8_t *p)
{
    return (uint16_t) (((uint16_t) p[0] << 8) | p[1]);
}

static uint32_t readBe24(const uint8_t *p)
{
    return ((uint32_t) p[0] << 16) | ((uint32_t) p[1] << 8) | p[2];
}

static void writeBe16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t) ((v >> 8) & 0xFF);
    p[1] = (uint8_t) (v & 0xFF);
}

static void writeBe24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t) ((v >> 16) & 0xFF);
    p[1] = (uint8_t) ((v >> 8) & 0xFF);
    p[2] = (uint8_t) (v & 0xFF);
}

static bool setTcpSequenceRandom(uint8_t *packet, uint32_t packet_length)
{
    if (packet_length < sizeof(struct ip_hdr))
    {
        return false;
    }

    struct ip_hdr *iph = (struct ip_hdr *) packet;
    if (IPH_V(iph) != 4 || IPH_PROTO(iph) != IPPROTO_TCP)
    {
        return false;
    }

    uint8_t ip_hlen = IPH_HL_BYTES(iph);
    if (ip_hlen < sizeof(struct ip_hdr) || packet_length < ip_hlen + sizeof(struct tcp_hdr))
    {
        return false;
    }

    struct tcp_hdr *tcph = (struct tcp_hdr *) (packet + ip_hlen);
    uint8_t         tcp_hlen = TCPH_HDRLEN_BYTES(tcph);
    if (tcp_hlen < sizeof(struct tcp_hdr) || packet_length < ip_hlen + tcp_hlen)
    {
        return false;
    }

    tcph->seqno = lwip_htonl(fastRand32());
    return true;
}

static sbuf_t *clonePacketWithLength(line_t *l, sbuf_t *buf, uint32_t new_len)
{
    buffer_pool_t *pool  = lineGetBufferPool(l);
    sbuf_t        *clone = NULL;

    if (new_len <= bufferpoolGetSmallBufferSize(pool))
    {
        clone = bufferpoolGetSmallBuffer(pool);
    }
    else if (new_len <= bufferpoolGetLargeBufferSize(pool))
    {
        clone = bufferpoolGetLargeBuffer(pool);
    }
    else
    {
        clone = sbufCreateWithPadding(new_len, sbufGetLeftPadding(buf));
    }

    sbufSetLength(clone, new_len);
    return clone;
}

static bool parseClientHelloSni(const uint8_t *packet, uint32_t packet_length, echosni_match_t *match)
{
    if (packet_length < sizeof(struct ip_hdr))
    {
        return false;
    }

    const struct ip_hdr *ipheader = (const struct ip_hdr *) packet;
    if (IPH_V(ipheader) != 4 || IPH_PROTO(ipheader) != IPPROTO_TCP)
    {
        return false;
    }

    uint8_t ip_hdr_len_words = IPH_HL(ipheader);
    if (ip_hdr_len_words < 5 || ip_hdr_len_words > 15)
    {
        LOGE("echosnitrick: invalid IP header length: %u", ip_hdr_len_words);
        return false;
    }

    uint16_t iphdr_len = (uint16_t) (ip_hdr_len_words * 4);
    if (packet_length < iphdr_len + sizeof(struct tcp_hdr))
    {
        return false;
    }

    uint16_t ip_total_len = lwip_ntohs(IPH_LEN(ipheader));
    if (ip_total_len < iphdr_len + sizeof(struct tcp_hdr) || packet_length < ip_total_len)
    {
        return false;
    }

    uint16_t off_f = lwip_ntohs(IPH_OFFSET(ipheader));
    if ((off_f & IP_OFFMASK) != 0)
    {
        return false;
    }

    const struct tcp_hdr *tcp_header = (const struct tcp_hdr *) (packet + iphdr_len);
    uint8_t               tcp_hdr_len_words = TCPH_HDRLEN(tcp_header);
    if (tcp_hdr_len_words < 5 || tcp_hdr_len_words > 15)
    {
        LOGE("echosnitrick: invalid TCP header length: %u", tcp_hdr_len_words);
        return false;
    }

    uint16_t tcphdr_len = (uint16_t) (tcp_hdr_len_words * 4);
    if (ip_total_len < iphdr_len + tcphdr_len + 9)
    {
        return false;
    }

    const uint8_t *tls             = packet + iphdr_len + tcphdr_len;
    uint16_t       tcp_payload_len = (uint16_t) (ip_total_len - iphdr_len - tcphdr_len);
    if (tls[0] != 0x16 || tls[1] != 0x03 || tls[2] > 0x03)
    {
        return false;
    }

    uint16_t tls_record_len = readBe16(tls + 3);
    if ((uint32_t) tls_record_len + 5U > tcp_payload_len)
    {
        return false;
    }

    if (tls[5] != 0x01)
    {
        return false;
    }

    uint32_t client_hello_len = readBe24(tls + 6);
    if (client_hello_len < 34 || client_hello_len + 4U > tls_record_len)
    {
        return false;
    }

    const uint8_t *client_hello = tls + 9;
    const uint8_t *cursor       = client_hello + 34;
    const uint8_t *hello_end    = client_hello + client_hello_len;

    if (cursor >= hello_end)
    {
        return false;
    }

    uint8_t session_id_len = cursor[0];
    cursor += 1;
    if ((size_t) (hello_end - cursor) < session_id_len + 2U)
    {
        return false;
    }
    cursor += session_id_len;

    uint16_t cipher_suites_len = readBe16(cursor);
    cursor += 2;
    if ((size_t) (hello_end - cursor) < cipher_suites_len + 1U)
    {
        return false;
    }
    cursor += cipher_suites_len;

    uint8_t compression_methods_len = cursor[0];
    cursor += 1;
    if ((size_t) (hello_end - cursor) < compression_methods_len + 2U)
    {
        return false;
    }
    cursor += compression_methods_len;

    uint16_t extensions_len = readBe16(cursor);
    cursor += 2;
    if ((size_t) (hello_end - cursor) < extensions_len)
    {
        return false;
    }

    const uint8_t *extensions_end = cursor + extensions_len;
    while (cursor + 4 <= extensions_end)
    {
        uint16_t extension_type = readBe16(cursor);
        uint16_t extension_len  = readBe16(cursor + 2);
        const uint8_t *extension_data = cursor + 4;
        const uint8_t *next_extension = extension_data + extension_len;

        if (next_extension > extensions_end)
        {
            return false;
        }

        if (extension_type == 0x0000)
        {
            if (extension_len < 2)
            {
                return false;
            }

            uint16_t       server_name_list_len = readBe16(extension_data);
            const uint8_t *server_name_cursor   = extension_data + 2;
            const uint8_t *server_name_list_end = server_name_cursor + server_name_list_len;

            if (server_name_list_end > next_extension)
            {
                return false;
            }

            while (server_name_cursor + 3 <= server_name_list_end)
            {
                uint8_t  name_type = server_name_cursor[0];
                uint16_t name_len  = readBe16(server_name_cursor + 1);
                const uint8_t *name_data = server_name_cursor + 3;
                const uint8_t *next_name = name_data + name_len;

                if (next_name > server_name_list_end)
                {
                    return false;
                }

                if (name_type == 0x00)
                {
                    *match = (echosni_match_t) {
                        .ip_total_len                     = ip_total_len,
                        .tls_record_len                   = tls_record_len,
                        .client_hello_len                = client_hello_len,
                        .server_name_list_len            = server_name_list_len,
                        .server_name_ext_len             = extension_len,
                        .sni_name_len                    = name_len,
                        .sni_name_offset                 = (uint32_t) (name_data - packet),
                        .sni_name_len_field_offset       = (uint32_t) ((server_name_cursor + 1) - packet),
                        .server_name_list_len_field_offset = (uint32_t) (extension_data - packet),
                        .server_name_ext_len_field_offset  = (uint32_t) ((cursor + 2) - packet),
                        .tls_record_len_field_offset       = (uint32_t) ((tls + 3) - packet),
                        .client_hello_len_field_offset     = (uint32_t) ((tls + 6) - packet),
                    };
                    return true;
                }

                server_name_cursor = next_name;
            }

            return false;
        }

        cursor = next_extension;
    }

    return false;
}

static sbuf_t *craftEchoSniPacket(tunnel_t *t, line_t *l, sbuf_t *buf, const echosni_match_t *match)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);
    uint32_t                original_len = sbufGetLength(buf);
    int32_t                 delta        = (int32_t) state->trick_echo_sni_first_sni_len - (int32_t) match->sni_name_len;
    int32_t                 new_ip_total_len = (int32_t) match->ip_total_len + delta;
    int32_t                 new_server_name_list_len = (int32_t) match->server_name_list_len + delta;
    int32_t                 new_server_name_ext_len  = (int32_t) match->server_name_ext_len + delta;
    int32_t                 new_tls_record_len       = (int32_t) match->tls_record_len + delta;
    int64_t                 new_client_hello_len     = (int64_t) match->client_hello_len + delta;

    if (delta > 0 && original_len > UINT32_MAX - (uint32_t) delta)
    {
        LOGW("echosnitrick: packet length overflow while expanding fake SNI");
        return NULL;
    }

    if (delta < 0 && original_len < (uint32_t) (-delta))
    {
        return NULL;
    }

    if (delta > 0 && match->ip_total_len > UINT16_MAX - (uint16_t) delta)
    {
        LOGW("echosnitrick: IPv4 total length overflow while expanding fake SNI");
        return NULL;
    }

    if (delta > 0 && match->server_name_list_len > UINT16_MAX - (uint16_t) delta)
    {
        LOGW("echosnitrick: TLS server-name list length overflow while expanding fake SNI");
        return NULL;
    }

    if (delta > 0 && match->server_name_ext_len > UINT16_MAX - (uint16_t) delta)
    {
        LOGW("echosnitrick: TLS server-name extension length overflow while expanding fake SNI");
        return NULL;
    }

    if (delta > 0 && match->tls_record_len > UINT16_MAX - (uint16_t) delta)
    {
        LOGW("echosnitrick: TLS record length overflow while expanding fake SNI");
        return NULL;
    }

    if (delta > 0 && match->client_hello_len > 0xFFFFFFU - (uint32_t) delta)
    {
        LOGW("echosnitrick: TLS ClientHello length overflow while expanding fake SNI");
        return NULL;
    }

    if (new_ip_total_len <= 0 || new_server_name_list_len <= 0 || new_server_name_ext_len <= 0 ||
        new_tls_record_len <= 0 || new_client_hello_len <= 0)
    {
        LOGW("echosnitrick: fake SNI would make packet lengths invalid");
        return NULL;
    }

    uint32_t new_len = original_len + (uint32_t) delta;
    if (delta < 0)
    {
        new_len = original_len - (uint32_t) (-delta);
    }

    sbuf_t       *clone      = clonePacketWithLength(l, buf, new_len);
    const uint8_t *source    = (const uint8_t *) sbufGetRawPtr(buf);
    uint8_t      *dest       = sbufGetMutablePtr(clone);
    uint32_t      prefix_len = match->sni_name_offset;
    uint32_t      tail_offset = match->sni_name_offset + match->sni_name_len;
    uint32_t      tail_len    = original_len - tail_offset;

    memoryCopyLarge(dest, source, prefix_len);
    memoryCopyLarge(dest + prefix_len, state->trick_echo_sni_first_sni, state->trick_echo_sni_first_sni_len);
    memoryCopyLarge(dest + prefix_len + state->trick_echo_sni_first_sni_len, source + tail_offset, tail_len);

    writeBe16(dest + match->sni_name_len_field_offset, state->trick_echo_sni_first_sni_len);
    writeBe16(dest + match->server_name_list_len_field_offset, (uint16_t) new_server_name_list_len);
    writeBe16(dest + match->server_name_ext_len_field_offset, (uint16_t) new_server_name_ext_len);
    writeBe16(dest + match->tls_record_len_field_offset, (uint16_t) new_tls_record_len);
    writeBe24(dest + match->client_hello_len_field_offset, (uint32_t) new_client_hello_len);

    struct ip_hdr *ipheader = (struct ip_hdr *) dest;
    IPH_LEN_SET(ipheader, lwip_htons((uint16_t) new_ip_total_len));

    return clone;
}

static void prepareEchoSniPacketForSend(tunnel_t *t, sbuf_t *packet)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);

    if (state->trick_echo_sni_ttl >= 0)
    {
        struct ip_hdr *fake_ipheader = (struct ip_hdr *) sbufGetMutablePtr(packet);
        IPH_TTL_SET(fake_ipheader, (uint8_t) state->trick_echo_sni_ttl);
    }

    if (state->trick_echo_sni_random_tcp_sequence)
    {
        setTcpSequenceRandom(sbufGetMutablePtr(packet), sbufGetLength(packet));
    }
}

static void echosnitrickSendEchoPacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    prepareEchoSniPacketForSend(t, buf);
    lineSetRecalculateChecksum(l, true);
    ipmanipulatorSendUpstreamFinal(t, l, buf);
}

static void echosnitrickSendOriginalPacket(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    /*
        Packet lines are shared worker state, so delayed sends cannot rely on the
        current recalculate_checksum scratch flag still belonging to this packet.
        Recomputing here is safe for the original packet and preserves correctness
        if earlier tricks in the same pass already changed it.
    */
    lineSetRecalculateChecksum(l, true);
    ipmanipulatorSendUpstreamFinal(t, l, buf);
}

static void echosnitrickSendLastEchoThenOriginal(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);
    echosni_match_t         match = {0};

    if (parseClientHelloSni((const uint8_t *) sbufGetRawPtr(buf), sbufGetLength(buf), &match))
    {
        sbuf_t *fake_packet = craftEchoSniPacket(t, l, buf, &match);
        if (fake_packet != NULL)
        {
            echosnitrickSendEchoPacket(t, l, fake_packet);

            if (! lineIsAlive(l))
            {
                reuseBuffer(buf);
                return;
            }
        }
    }

    if (state->trick_echo_sni_final_delay_ms > 0)
    {
        lineScheduleDelayedTaskWithBuf(l, echosnitrickSendOriginalPacket, state->trick_echo_sni_final_delay_ms, t, buf);
        return;
    }

    echosnitrickSendOriginalPacket(t, l, buf);
}

bool echosnitrickUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);
    echosni_match_t         match = {0};
    buffer_pool_t          *pool = lineGetBufferPool(l);

    if (! parseClientHelloSni((const uint8_t *) sbufGetRawPtr(buf), sbufGetLength(buf), &match))
    {
        return false;
    }

    sbuf_t *fake_packet = craftEchoSniPacket(t, l, buf, &match);
    if (fake_packet == NULL)
    {
        return false;
    }

    LOGD("IpManipulator: EchoSNI injected SNI \"%s\" %u time(s) before forwarding the original ClientHello",
         state->trick_echo_sni_first_sni, state->trick_echo_sni_count);

    lineLock(l);

    echosnitrickSendEchoPacket(t, l, fake_packet);

    if (! lineIsAlive(l))
    {
        bufferpoolReuseBuffer(pool, buf);
        lineUnlock(l);
        return true;
    }

    if (state->trick_echo_sni_count == 1)
    {
        if (state->trick_echo_sni_final_delay_ms > 0)
        {
            lineScheduleDelayedTaskWithBuf(l, echosnitrickSendOriginalPacket, state->trick_echo_sni_final_delay_ms, t,
                                           buf);
            lineUnlock(l);
            return true;
        }

        echosnitrickSendOriginalPacket(t, l, buf);
        lineUnlock(l);
        return true;
    }

    if (state->trick_echo_sni_replay_delay_ms == 0)
    {
        for (uint32_t echo_index = 1; echo_index < state->trick_echo_sni_count; ++echo_index)
        {
            sbuf_t *extra_echo = craftEchoSniPacket(t, l, buf, &match);
            if (extra_echo == NULL)
            {
                break;
            }

            echosnitrickSendEchoPacket(t, l, extra_echo);
            if (! lineIsAlive(l))
            {
                reuseBuffer(buf);
                lineUnlock(l);
                return true;
            }
        }

        if (state->trick_echo_sni_final_delay_ms > 0)
        {
            lineScheduleDelayedTaskWithBuf(l, echosnitrickSendOriginalPacket, state->trick_echo_sni_final_delay_ms, t,
                                           buf);
            lineUnlock(l);
            return true;
        }

        echosnitrickSendOriginalPacket(t, l, buf);
        lineUnlock(l);
        return true;
    }

    for (uint32_t echo_index = 1; echo_index + 1 < state->trick_echo_sni_count; ++echo_index)
    {
        sbuf_t *scheduled_echo = craftEchoSniPacket(t, l, buf, &match);
        if (scheduled_echo == NULL)
        {
            break;
        }

        uint32_t delay_ms = echo_index * state->trick_echo_sni_replay_delay_ms;
        lineScheduleDelayedTaskWithBuf(l, echosnitrickSendEchoPacket, delay_ms, t, scheduled_echo);
    }

    lineScheduleDelayedTaskWithBuf(l, echosnitrickSendLastEchoThenOriginal,
                                   (state->trick_echo_sni_count - 1) * state->trick_echo_sni_replay_delay_ms, t, buf);

    lineUnlock(l);
    return true;
}
