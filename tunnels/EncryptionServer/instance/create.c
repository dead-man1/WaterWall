#include "structure.h"

#include "loggers/network_logger.h"

tunnel_t *encryptionserverTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(encryptionserver_tstate_t), sizeof(encryptionserver_lstate_t));

    t->fnInitU    = &encryptionserverTunnelUpStreamInit;
    t->fnEstU     = &encryptionserverTunnelUpStreamEst;
    t->fnFinU     = &encryptionserverTunnelUpStreamFinish;
    t->fnPayloadU = &encryptionserverTunnelUpStreamPayload;
    t->fnPauseU   = &encryptionserverTunnelUpStreamPause;
    t->fnResumeU  = &encryptionserverTunnelUpStreamResume;

    t->fnInitD    = &encryptionserverTunnelDownStreamInit;
    t->fnEstD     = &encryptionserverTunnelDownStreamEst;
    t->fnFinD     = &encryptionserverTunnelDownStreamFinish;
    t->fnPayloadD = &encryptionserverTunnelDownStreamPayload;
    t->fnPauseD   = &encryptionserverTunnelDownStreamPause;
    t->fnResumeD  = &encryptionserverTunnelDownStreamResume;

    t->onPrepare = &encryptionserverTunnelOnPrepair;
    t->onStart   = &encryptionserverTunnelOnStart;
    t->onDestroy = &encryptionserverTunnelDestroy;

    encryptionserver_tstate_t *ts       = tunnelGetState(t);
    const cJSON               *settings = node->node_settings_json;

    if (! encryptionserverTunnelstateInitialize(ts, settings))
    {
        tunnelDestroy(t);
        return NULL;
    }

    return t;
}
