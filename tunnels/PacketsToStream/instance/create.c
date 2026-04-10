#include "structure.h"

#include "loggers/network_logger.h"

tunnel_t *packetstostreamTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(packetstostream_tstate_t), sizeof(packetstostream_lstate_t));

    t->fnInitU    = &packetstostreamTunnelUpStreamInit;
    t->fnEstU     = &packetstostreamTunnelUpStreamEst;
    t->fnFinU     = &packetstostreamTunnelUpStreamFinish;
    t->fnPayloadU = &packetstostreamTunnelUpStreamPayload;
    t->fnPauseU   = &packetstostreamTunnelUpStreamPause;
    t->fnResumeU  = &packetstostreamTunnelUpStreamResume;

    t->fnInitD    = &packetstostreamTunnelDownStreamInit;
    t->fnEstD     = &packetstostreamTunnelDownStreamEst;
    t->fnFinD     = &packetstostreamTunnelDownStreamFinish;
    t->fnPayloadD = &packetstostreamTunnelDownStreamPayload;
    t->fnPauseD   = &packetstostreamTunnelDownStreamPause;
    t->fnResumeD  = &packetstostreamTunnelDownStreamResume;

    t->onPrepare = &packetstostreamTunnelOnPrepair;
    t->onStart   = &packetstostreamTunnelOnStart;
    t->onDestroy = &packetstostreamTunnelDestroy;
    
    return t;
}
