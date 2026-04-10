#include "structure.h"

#include "loggers/network_logger.h"

static bool ipoverriderParseRuleAddress(ipoverrider_rule_t *rule, const cJSON *settings, const char *json_path)
{
    char *ipbuf = NULL;

    if (getStringFromJsonObject(&ipbuf, settings, "ipv4"))
    {
        sockaddr_u sa;
        rule->support4 = true;
        sockaddrSetIpAddress(&(sa), ipbuf);
        memoryCopy(&(rule->ov_4), &(sa.sin.sin_addr.s_addr), sizeof(sa.sin.sin_addr.s_addr));
        memoryFree(ipbuf);
        return true;
    }

    if (getStringFromJsonObject(&ipbuf, settings, "ipv6"))
    {
        sockaddr_u sa;
        rule->support6 = true;
        sockaddrSetIpAddress(&(sa), ipbuf);
        memoryCopy(&(rule->ov_6), &(sa.sin6.sin6_addr.s6_addr), sizeof(sa.sin6.sin6_addr.s6_addr));
        memoryFree(ipbuf);
        return true;
    }

    LOGF("JSON Error: %s : please give the ip, use ipv4 or ipv6 json keys", json_path);
    return false;
}

static bool ipoverriderParseRule(ipoverrider_rule_t *rule, const cJSON *settings, const char *json_path)
{
    if (! checkJsonIsObjectAndHasChild(settings))
    {
        LOGF("JSON Error: %s (object field) : The object was empty or invalid", json_path);
        return false;
    }

    if (getIntFromJsonObjectOrDefault(&rule->skip_chance, settings, "chance", -1))
    {
        if (rule->skip_chance < 0 || rule->skip_chance > 100)
        {
            LOGF("JSON Error: %s->chance (int field) : chance is less than 0 or more than 100", json_path);
            return false;
        }

        rule->skip_chance = 100 - rule->skip_chance;
    }

    getBoolFromJsonObjectOrDefault(&rule->only120, settings, "only120", false);

    if (! ipoverriderParseRuleAddress(rule, settings, json_path))
    {
        return false;
    }

    rule->enabled = true;
    return true;
}

static bool ipoverriderParseLegacySettings(ipoverrider_tstate_t *state, const cJSON *settings)
{
    dynamic_value_t directon_dv = parseDynamicNumericValueFromJsonObject(settings, "direction", 2, "up", "down");
    if (directon_dv.status != kDvsUp && directon_dv.status != kDvsDown)
    {
        LOGF("IpOverrider: IpOverrider->settings->direction (string field)  must be either up or down ");
        terminateProgram(1);
    }

    dynamic_value_t mode_dv = parseDynamicNumericValueFromJsonObject(settings, "mode", 2, "source-ip", "dest-ip");
    if (mode_dv.status != kDvsDestMode && mode_dv.status != kDvsSourceMode)
    {
        LOGF("IpOverrider: IpOverrider->settings->mode (string field)  mode is not set or invalid, do you "
             "want to override source ip or dest ip?");
        terminateProgram(1);
    }

    uint8_t direction_index =
        (directon_dv.status == kDvsUp) ? kIpOverriderDirectionUp : kIpOverriderDirectionDown;
    uint8_t mode_index = (mode_dv.status == kDvsSourceMode) ? kIpOverriderModeSource : kIpOverriderModeDest;

    dynamicvalueDestroy(directon_dv);
    dynamicvalueDestroy(mode_dv);
    return ipoverriderParseRule(&(state->rules[direction_index][mode_index]), settings, "IpOverrider->settings");
}

static bool ipoverriderParseDirectionalSettings(ipoverrider_tstate_t *state, const cJSON *settings)
{
    const char *direction_keys[kIpOverriderDirectionCount] = {"up", "down"};
    const char *mode_keys[kIpOverriderModeCount]           = {"source-ip", "dest-ip"};
    const char *json_paths[kIpOverriderDirectionCount][kIpOverriderModeCount] = {
        {"IpOverrider->settings->up->source-ip", "IpOverrider->settings->up->dest-ip"},
        {"IpOverrider->settings->down->source-ip", "IpOverrider->settings->down->dest-ip"},
    };

    bool parsed_any_rule = false;

    for (uint8_t direction_index = 0; direction_index < kIpOverriderDirectionCount; ++direction_index)
    {
        const cJSON *direction_settings = cJSON_GetObjectItemCaseSensitive(settings, direction_keys[direction_index]);
        if (direction_settings == NULL)
        {
            continue;
        }

        if (! checkJsonIsObjectAndHasChild(direction_settings))
        {
            LOGF("JSON Error: IpOverrider->settings->%s (object field) : The object was empty or invalid",
                 direction_keys[direction_index]);
            return false;
        }

        for (uint8_t mode_index = 0; mode_index < kIpOverriderModeCount; ++mode_index)
        {
            const cJSON *rule_settings = cJSON_GetObjectItemCaseSensitive(direction_settings, mode_keys[mode_index]);
            if (rule_settings == NULL)
            {
                continue;
            }

            if (! ipoverriderParseRule(&(state->rules[direction_index][mode_index]), rule_settings,
                                       json_paths[direction_index][mode_index]))
            {
                return false;
            }

            parsed_any_rule = true;
        }
    }

    if (! parsed_any_rule)
    {
        LOGF("JSON Error: IpOverrider->settings : please provide at least one of up/down source-ip or dest-ip rules");
    }

    return parsed_any_rule;
}

tunnel_t *ipoverriderCreate(node_t *node)
{
    tunnel_t *t = packettunnelCreate(node, sizeof(ipoverrider_tstate_t), 0);

    t->fnPayloadU = &ipoverriderUpStreamPayload;
    t->fnPayloadD = &ipoverriderDownStreamPayload;
    t->onPrepare  = &ipoverriderOnPrepair;
    t->onStart    = &ipoverriderOnStart;
    t->onDestroy  = &ipoverriderDestroy;

    ipoverrider_tstate_t *state = tunnelGetState(t);

    const cJSON *settings = node->node_settings_json;

    if (! (cJSON_IsObject(settings) && settings->child != NULL))
    {
        LOGF("JSON Error: IpOverrider->settings (object field) : The object was empty or invalid");
        return NULL;
    }

    if (cJSON_GetObjectItemCaseSensitive(settings, "up") != NULL ||
        cJSON_GetObjectItemCaseSensitive(settings, "down") != NULL)
    {
        if (! ipoverriderParseDirectionalSettings(state, settings))
        {
            return NULL;
        }
    }
    else if (! ipoverriderParseLegacySettings(state, settings))
    {
        return NULL;
    }

    return t;
}
