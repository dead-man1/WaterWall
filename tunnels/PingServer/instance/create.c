#include "structure.h"

#include "loggers/network_logger.h"

static bool pingserverParseIpv4String(uint32_t *dest, const char *ipbuf, const char *json_path)
{
    ip4_addr_t parsed_ipv4;

    if (ip4AddrAddressToNetwork(ipbuf, &parsed_ipv4) == 0)
    {
        LOGF("JSON Error: %s (string field) : expected a single IPv4 address", json_path);
        return false;
    }

    *dest = ip4AddrGetU32(&parsed_ipv4);
    return true;
}

static bool pingserverLoadIpv4Setting(uint32_t *dest, const cJSON *settings, const char *key, const char *json_path)
{
    char *ip_string = NULL;

    if (! getStringFromJsonObject(&ip_string, settings, key))
    {
        LOGF("JSON Error: %s (string field) : The data was empty or invalid", json_path);
        return false;
    }

    const bool ok = pingserverParseIpv4String(dest, ip_string, json_path);
    memoryFree(ip_string);
    return ok;
}

static bool pingserverLoadUint16Setting(uint16_t *dest, const cJSON *settings, const char *key, int default_value,
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

static bool pingserverLoadUint8Setting(uint8_t *dest, const cJSON *settings, const char *key, int default_value,
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

tunnel_t *pingserverCreate(node_t *node)
{
    tunnel_t *t = packettunnelCreate(node, sizeof(pingserver_tstate_t), 0);

    t->fnPayloadU = &pingserverUpStreamPayload;
    t->fnPayloadD = &pingserverDownStreamPayload;
    t->onPrepare  = &pingserverOnPrepair;
    t->onStart    = &pingserverOnStart;
    t->onDestroy  = &pingserverDestroy;

    pingserver_tstate_t *state    = tunnelGetState(t);
    const cJSON         *settings = node->node_settings_json;

    if (! (cJSON_IsObject(settings) && settings->child != NULL))
    {
        LOGF("JSON Error: PingServer->settings (object field) : The object was empty or invalid");
        pingserverDestroy(t);
        return NULL;
    }

    if (! pingserverLoadIpv4Setting(&state->local_ipv4, settings, "local-ip", "PingServer->settings->local-ip") ||
        ! pingserverLoadIpv4Setting(&state->peer_ipv4, settings, "peer-ip", "PingServer->settings->peer-ip") ||
        ! pingserverLoadUint16Setting(&state->identifier, settings, "identifier", kPingServerDefaultIdentifier,
                                      "PingServer->settings->identifier") ||
        ! pingserverLoadUint8Setting(&state->ttl, settings, "ttl", kPingServerDefaultTtl, "PingServer->settings->ttl") ||
        ! pingserverLoadUint8Setting(&state->tos, settings, "tos", 0, "PingServer->settings->tos"))
    {
        pingserverDestroy(t);
        return NULL;
    }

    uint16_t sequence_start = 0;
    uint16_t ipv4_id_start  = 0;

    if (! pingserverLoadUint16Setting(&sequence_start, settings, "sequence-start", 0,
                                      "PingServer->settings->sequence-start") ||
        ! pingserverLoadUint16Setting(&ipv4_id_start, settings, "ipv4-id-start", 0,
                                      "PingServer->settings->ipv4-id-start"))
    {
        pingserverDestroy(t);
        return NULL;
    }

    atomicStoreRelaxed(&state->icmp_sequence, sequence_start);
    atomicStoreRelaxed(&state->ipv4_identification, ipv4_id_start);

    return t;
}
