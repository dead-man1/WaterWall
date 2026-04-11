#include "structure.h"

#include "loggers/network_logger.h"

tunnel_t *encryptionclientTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(encryptionclient_tstate_t), sizeof(encryptionclient_lstate_t));

    t->fnInitU    = &encryptionclientTunnelUpStreamInit;
    t->fnEstU     = &encryptionclientTunnelUpStreamEst;
    t->fnFinU     = &encryptionclientTunnelUpStreamFinish;
    t->fnPayloadU = &encryptionclientTunnelUpStreamPayload;
    t->fnPauseU   = &encryptionclientTunnelUpStreamPause;
    t->fnResumeU  = &encryptionclientTunnelUpStreamResume;

    t->fnInitD    = &encryptionclientTunnelDownStreamInit;
    t->fnEstD     = &encryptionclientTunnelDownStreamEst;
    t->fnFinD     = &encryptionclientTunnelDownStreamFinish;
    t->fnPayloadD = &encryptionclientTunnelDownStreamPayload;
    t->fnPauseD   = &encryptionclientTunnelDownStreamPause;
    t->fnResumeD  = &encryptionclientTunnelDownStreamResume;

    t->onPrepare = &encryptionclientTunnelOnPrepair;
    t->onStart   = &encryptionclientTunnelOnStart;
    t->onDestroy = &encryptionclientTunnelDestroy;

    encryptionclient_tstate_t *ts       = tunnelGetState(t);
    const cJSON               *settings = node->node_settings_json;

    if (! encryptionclientTunnelstateInitialize(ts, settings))
    {
        tunnelDestroy(t);
        return NULL;
    }

    return t;
}
