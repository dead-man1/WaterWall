#include "structure.h"

#include "loggers/network_logger.h"

tunnel_t *streamtopacketsTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(streamtopackets_tstate_t), sizeof(streamtopackets_lstate_t));

    t->fnInitU    = &streamtopacketsTunnelUpStreamInit;
    t->fnEstU     = &streamtopacketsTunnelUpStreamEst;
    t->fnFinU     = &streamtopacketsTunnelUpStreamFinish;
    t->fnPayloadU = &streamtopacketsTunnelUpStreamPayload;
    t->fnPauseU   = &streamtopacketsTunnelUpStreamPause;
    t->fnResumeU  = &streamtopacketsTunnelUpStreamResume;

    t->fnInitD    = &streamtopacketsTunnelDownStreamInit;
    t->fnEstD     = &streamtopacketsTunnelDownStreamEst;
    t->fnFinD     = &streamtopacketsTunnelDownStreamFinish;
    t->fnPayloadD = &streamtopacketsTunnelDownStreamPayload;
    t->fnPauseD   = &streamtopacketsTunnelDownStreamPause;
    t->fnResumeD  = &streamtopacketsTunnelDownStreamResume;

    t->onPrepare = &streamtopacketsTunnelOnPrepair;
    t->onStart   = &streamtopacketsTunnelOnStart;
    t->onDestroy = &streamtopacketsTunnelDestroy;
    
    return t;
}
