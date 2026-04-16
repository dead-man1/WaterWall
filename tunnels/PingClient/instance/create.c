#include "structure.h"

#include "loggers/network_logger.h"

static bool pingclientLoadUint16Setting(uint16_t *dest, const cJSON *settings, const char *key, int default_value,
                                        const char *json_path)
{
    int value = default_value;
    getIntFromJsonObjectOrDefault(&value, settings, key, default_value);

    if (value < 0 || value > UINT16_MAX)
    {
        LOGF("JSON Error: %s (int field) : expected a value between 0 and %u", json_path, (unsigned int) UINT16_MAX);
        return false;
    }

    *dest = (uint16_t) value;
    return true;
}

static bool pingclientLoadUint8Setting(uint8_t *dest, const cJSON *settings, const char *key, int default_value,
                                       const char *json_path)
{
    int value = default_value;
    getIntFromJsonObjectOrDefault(&value, settings, key, default_value);

    if (value < 0 || value > UINT8_MAX)
    {
        LOGF("JSON Error: %s (int field) : expected a value between 0 and %u", json_path, (unsigned int) UINT8_MAX);
        return false;
    }

    *dest = (uint8_t) value;
    return true;
}

static bool pingclientLoadOptionalXorByteSetting(bool *enabled_out, uint8_t *value_out, const cJSON *settings,
                                                 const char *key, const char *json_path)
{
    int value = -1;
    getIntFromJsonObjectOrDefault(&value, settings, key, -1);

    if (value < -1 || value > UINT8_MAX)
    {
        LOGF("JSON Error: %s (int field) : expected a value between 0 and %u, or omit the field",
             json_path, (unsigned int) UINT8_MAX);
        return false;
    }

    *enabled_out = (value != -1);
    *value_out   = (value == -1) ? 0 : (uint8_t) value;
    return true;
}

tunnel_t *pingclientCreate(node_t *node)
{
    tunnel_t *t = packettunnelCreate(node, sizeof(pingclient_tstate_t), 0);

    t->fnPayloadU = &pingclientUpStreamPayload;
    t->fnPayloadD = &pingclientDownStreamPayload;
    t->onPrepare  = &pingclientOnPrepair;
    t->onStart    = &pingclientOnStart;
    t->onDestroy  = &pingclientDestroy;

    pingclient_tstate_t *state    = tunnelGetState(t);
    const cJSON         *settings = node->node_settings_json;

    if (! (cJSON_IsObject(settings) && settings->child != NULL))
    {
        LOGF("JSON Error: PingClient->settings (object field) : The object was empty or invalid");
        pingclientDestroy(t);
        return NULL;
    }

    if (! pingclientLoadUint16Setting(&state->identifier, settings, "identifier", kPingClientDefaultIdentifier,
                                      "PingClient->settings->identifier") ||
        ! pingclientLoadUint8Setting(&state->ttl, settings, "ttl", kPingClientDefaultTtl, "PingClient->settings->ttl") ||
        ! pingclientLoadUint8Setting(&state->tos, settings, "tos", 0, "PingClient->settings->tos") ||
        ! pingclientLoadOptionalXorByteSetting(&state->payload_xor_enabled, &state->payload_xor_byte, settings,
                                               "xor-byte", "PingClient->settings->xor-byte"))
    {
        pingclientDestroy(t);
        return NULL;
    }

    getBoolFromJsonObjectOrDefault(&state->roundup_payload_size, settings, "roundup-size", false);

    if (! state->roundup_payload_size)
    {
        getBoolFromJsonObjectOrDefault(&state->roundup_payload_size, settings, "roundup", false);
    }

    uint16_t sequence_start = 0;
    uint16_t ipv4_id_start  = 0;

    if (! pingclientLoadUint16Setting(&sequence_start, settings, "sequence-start", 0,
                                      "PingClient->settings->sequence-start") ||
        ! pingclientLoadUint16Setting(&ipv4_id_start, settings, "ipv4-id-start", 0,
                                      "PingClient->settings->ipv4-id-start"))
    {
        pingclientDestroy(t);
        return NULL;
    }

    atomicStoreRelaxed(&state->icmp_sequence, sequence_start);
    atomicStoreRelaxed(&state->ipv4_identification, ipv4_id_start);

    return t;
}
