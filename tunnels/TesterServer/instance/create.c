#include "structure.h"

#include "loggers/network_logger.h"

tunnel_t *testerserverTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(testerserver_tstate_t), sizeof(testerserver_lstate_t));

    t->fnInitU    = &testerserverTunnelUpStreamInit;
    t->fnEstU     = &testerserverTunnelUpStreamEst;
    t->fnFinU     = &testerserverTunnelUpStreamFinish;
    t->fnPayloadU = &testerserverTunnelUpStreamPayload;
    t->fnPauseU   = &testerserverTunnelUpStreamPause;
    t->fnResumeU  = &testerserverTunnelUpStreamResume;

    t->fnInitD    = &testerserverTunnelDownStreamInit;
    t->fnEstD     = &testerserverTunnelDownStreamEst;
    t->fnFinD     = &testerserverTunnelDownStreamFinish;
    t->fnPayloadD = &testerserverTunnelDownStreamPayload;
    t->fnPauseD   = &testerserverTunnelDownStreamPause;
    t->fnResumeD  = &testerserverTunnelDownStreamResume;

    t->onPrepare = &testerserverTunnelOnPrepair;
    t->onStart   = &testerserverTunnelOnStart;
    t->onDestroy = &testerserverTunnelDestroy;

    testerserver_tstate_t *ts       = tunnelGetState(t);
    const cJSON           *settings = node->node_settings_json;

    getBoolFromJsonObjectOrDefault(&ts->packet_mode, settings, "packet-mode", false);

    return t;
}
