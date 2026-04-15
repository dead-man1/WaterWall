#include "structure.h"

#include "loggers/network_logger.h"

tunnel_t *testerclientTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(testerclient_tstate_t), sizeof(testerclient_lstate_t));

    t->fnInitU    = &testerclientTunnelUpStreamInit;
    t->fnEstU     = &testerclientTunnelUpStreamEst;
    t->fnFinU     = &testerclientTunnelUpStreamFinish;
    t->fnPayloadU = &testerclientTunnelUpStreamPayload;
    t->fnPauseU   = &testerclientTunnelUpStreamPause;
    t->fnResumeU  = &testerclientTunnelUpStreamResume;

    t->fnInitD    = &testerclientTunnelDownStreamInit;
    t->fnEstD     = &testerclientTunnelDownStreamEst;
    t->fnFinD     = &testerclientTunnelDownStreamFinish;
    t->fnPayloadD = &testerclientTunnelDownStreamPayload;
    t->fnPauseD   = &testerclientTunnelDownStreamPause;
    t->fnResumeD  = &testerclientTunnelDownStreamResume;

    t->onPrepare = &testerclientTunnelOnPrepair;
    t->onStart   = &testerclientTunnelOnStart;
    t->onDestroy = &testerclientTunnelDestroy;

    testerclient_tstate_t *ts       = tunnelGetState(t);
    const cJSON           *settings = node->node_settings_json;

    getBoolFromJsonObjectOrDefault(&ts->packet_mode, settings, "packet-mode", false);

    return t;
}
