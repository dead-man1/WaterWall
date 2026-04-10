#include "structure.h"

#include "loggers/network_logger.h"

static bool ipoverriderShouldSkipRule(const ipoverrider_rule_t *rule)
{
    return rule->skip_chance != -1 && (fastRand32() % 100) < (uint32_t) rule->skip_chance;
}

static void ipoverriderApplyRule(const ipoverrider_rule_t *rule, line_t *l, sbuf_t *buf, bool replace_source)
{
    if (! rule->enabled || ipoverriderShouldSkipRule(rule))
    {
        return;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (rule->support4 && IPH_V(ipheader) == 4)
    {
        uint16_t size = lwip_ntohs(IPH_LEN(ipheader));

        if (rule->only120 && size > 120)
        {
            return;
        }

        if (replace_source)
        {
            memoryCopy(&(ipheader->src.addr), &(rule->ov_4), sizeof(rule->ov_4));
        }
        else
        {
            memoryCopy(&(ipheader->dest.addr), &(rule->ov_4), sizeof(rule->ov_4));
        }

        l->recalculate_checksum = true;
    }
    // else if (rule->support6 && IPH_V(ipheader) == 6)
    // {
    //     struct ip6_hdr *ip6header = (struct ip6_hdr *) sbufGetMutablePtr(buf);
    //     // alignment assumed to be correct
    //     if (replace_source)
    //     {
    //         memoryCopy(&(ip6header->src.addr), &(rule->ov_6), sizeof(rule->ov_6));
    //     }
    //     else
    //     {
    //         memoryCopy(&(ip6header->dest.addr), &(rule->ov_6), sizeof(rule->ov_6));
    //     }
    // }
}

void ipoverriderApplyUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipoverrider_tstate_t *state = tunnelGetState(t);

    ipoverriderApplyRule(&(state->rules[kIpOverriderDirectionUp][kIpOverriderModeSource]), l, buf, true);
    ipoverriderApplyRule(&(state->rules[kIpOverriderDirectionUp][kIpOverriderModeDest]), l, buf, false);

    tunnelNextUpStreamPayload(t, l, buf);
}

void ipoverriderApplyDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    ipoverrider_tstate_t *state = tunnelGetState(t);

    ipoverriderApplyRule(&(state->rules[kIpOverriderDirectionDown][kIpOverriderModeSource]), l, buf, true);
    ipoverriderApplyRule(&(state->rules[kIpOverriderDirectionDown][kIpOverriderModeDest]), l, buf, false);

    tunnelPrevDownStreamPayload(t, l, buf);
}
