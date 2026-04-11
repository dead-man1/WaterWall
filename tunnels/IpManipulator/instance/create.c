#include "structure.h"

#include "loggers/network_logger.h"

#include "tricks/protoswap/trick.h"
#include "tricks/sniblender/trick.h"
#include "tricks/tcpbitchange/trick.h"

static bool parseTcpBitActionField(enum tcp_bit_action_dynamic_value *dest, const cJSON *settings, const char *key)
{
    dynamic_value_t action =
        parseDynamicStrValueFromJsonObject(settings, key, 11, "off", "on", "toggle", "packet->cwr", "packet->ece",
                                           "packet->urg", "packet->ack", "packet->psh", "packet->rst", "packet->syn",
                                           "packet->fin");

    if (action.status == kDvsConstant)
    {
        if (action.string != NULL && (stringCompare((const char *) action.string, "flip") == 0 ||
                                      stringCompare((const char *) action.string, "switch") == 0))
        {
            *dest = kDvsToggle;
            dynamicvalueDestroy(action);
            return true;
        }

        LOGF("IpManipulator: settings->%s has invalid value", key);
        dynamicvalueDestroy(action);
        return false;
    }

    *dest = (enum tcp_bit_action_dynamic_value) action.status;
    dynamicvalueDestroy(action);
    return true;
}

tunnel_t *ipmanipulatorCreate(node_t *node)
{
    tunnel_t *t = packettunnelCreate(node, sizeof(ipmanipulator_tstate_t), 0);

    t->fnPayloadU = &ipmanipulatorUpStreamPayload;
    t->fnPayloadD = &ipmanipulatorDownStreamPayload;
    t->onPrepare  = &ipmanipulatorOnPrepair;
    t->onStart    = &ipmanipulatorOnStart;
    t->onDestroy  = &ipmanipulatorDestroy;

    ipmanipulator_tstate_t *state = tunnelGetState(t);
    const cJSON            *settings = node->node_settings_json;

    state->trick_proto_swap_tcp_number = -1;
    state->trick_proto_swap_tcp_number_2 = -1;
    state->trick_proto_swap_udp_number = -1;
    state->trick_proto_swap_tcp_toggle_up = 0;
    state->trick_proto_swap_tcp_toggle_down = 0;

    bool has_proto_swap = false;
    has_proto_swap |= getIntFromJsonObject(&state->trick_proto_swap_tcp_number, settings, "protoswap");
    has_proto_swap |= getIntFromJsonObject(&state->trick_proto_swap_tcp_number, settings, "protoswap-tcp");
    has_proto_swap |= getIntFromJsonObject(&state->trick_proto_swap_udp_number, settings, "protoswap-udp");
    getIntFromJsonObjectOrDefault(&state->trick_proto_swap_tcp_number_2, settings, "protoswap-tcp-2", -1);
    state->trick_proto_swap = has_proto_swap;

    bool sni_blender_enabled = false;
    getBoolFromJsonObject(&sni_blender_enabled, settings, "sni-blender");
    if (sni_blender_enabled)
    {
        if (! getIntFromJsonObject(&state->trick_sni_blender_packets_count, settings, "sni-blender-packets"))
        {
            LOGF("IpManipulator: sni-blender is enabled but field \"sni-blender-packets\" is not set");
            tunnelDestroy(t);
            return NULL;
        }

        if (state->trick_sni_blender_packets_count <= 0)
        {
            LOGF("IpManipulator: sni-blender-packets must be greater than zero");
            tunnelDestroy(t);
            return NULL;
        }

        if (state->trick_sni_blender_packets_count > kSniBlenderTrickMaxPacketsCount)
        {
            LOGF("IpManipulator: sni-blender-packets cannot be more than %d", kSniBlenderTrickMaxPacketsCount);
            tunnelDestroy(t);
            return NULL;
        }

        state->trick_sni_blender = true;
    }

    bool tcp_parse_ok = true;
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_cwr_action, settings, "up-tcp-bit-cwr");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_ece_action, settings, "up-tcp-bit-ece");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_urg_action, settings, "up-tcp-bit-urg");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_ack_action, settings, "up-tcp-bit-ack");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_psh_action, settings, "up-tcp-bit-psh");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_rst_action, settings, "up-tcp-bit-rst");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_syn_action, settings, "up-tcp-bit-syn");
    tcp_parse_ok &= parseTcpBitActionField(&state->up_tcp_bit_fin_action, settings, "up-tcp-bit-fin");

    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_cwr_action, settings, "dw-tcp-bit-cwr");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_ece_action, settings, "dw-tcp-bit-ece");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_urg_action, settings, "dw-tcp-bit-urg");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_ack_action, settings, "dw-tcp-bit-ack");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_psh_action, settings, "dw-tcp-bit-psh");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_rst_action, settings, "dw-tcp-bit-rst");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_syn_action, settings, "dw-tcp-bit-syn");
    tcp_parse_ok &= parseTcpBitActionField(&state->down_tcp_bit_fin_action, settings, "dw-tcp-bit-fin");

    if (! tcp_parse_ok)
    {
        tunnelDestroy(t);
        return NULL;
    }

    state->trick_tcp_bit_changes =
        (state->down_tcp_bit_cwr_action != kDvsNoAction || state->down_tcp_bit_ece_action != kDvsNoAction ||
         state->down_tcp_bit_urg_action != kDvsNoAction || state->down_tcp_bit_ack_action != kDvsNoAction ||
         state->down_tcp_bit_psh_action != kDvsNoAction || state->down_tcp_bit_rst_action != kDvsNoAction ||
         state->down_tcp_bit_syn_action != kDvsNoAction || state->down_tcp_bit_fin_action != kDvsNoAction ||
         state->up_tcp_bit_cwr_action != kDvsNoAction || state->up_tcp_bit_ece_action != kDvsNoAction ||
         state->up_tcp_bit_urg_action != kDvsNoAction || state->up_tcp_bit_ack_action != kDvsNoAction ||
         state->up_tcp_bit_psh_action != kDvsNoAction || state->up_tcp_bit_rst_action != kDvsNoAction ||
         state->up_tcp_bit_syn_action != kDvsNoAction || state->up_tcp_bit_fin_action != kDvsNoAction);

    if (! (state->trick_proto_swap || state->trick_sni_blender || state->trick_tcp_bit_changes))
    {
        LOGF("IpManipulator: no tricks are enabled, nothing to do");
        tunnelDestroy(t);
        return NULL;
    }

    return t;
}
