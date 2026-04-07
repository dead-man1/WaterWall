#pragma once

#include "wwapi.h"

typedef struct disturber_tstate_s
{
    int chance_instant_close;
    int chance_middle_close;
    int chance_payload_corruption;
    int chance_payload_loss;
    int chance_payload_duplication;
    int chance_payload_out_of_order;
    int chance_payload_delay;
    int chance_connection_deadhang;
    int delay_min_ms;
    int delay_max_ms;

} disturber_tstate_t;

typedef struct disturber_lstate_s
{
    bool is_deadhang; // the connection is dead, no packet type will be transmitted, but only close

    sbuf_t *held_payload; // store  a payload and wait for next, to be able to send them out of order
} disturber_lstate_t;

enum
{
    kTunnelStateSize = sizeof(disturber_tstate_t),
    kLineStateSize   = sizeof(disturber_lstate_t)
};

WW_EXPORT void         disturberTunnelDestroy(tunnel_t *t);
WW_EXPORT tunnel_t    *disturberTunnelCreate(node_t *node);
WW_EXPORT api_result_t disturberTunnelApi(tunnel_t *instance, sbuf_t *message);

void disturberTunnelOnIndex(tunnel_t *t, uint16_t index, uint16_t *mem_offset);
void disturberTunnelOnChain(tunnel_t *t, tunnel_chain_t *chain);
void disturberTunnelOnPrepair(tunnel_t *t);
void disturberTunnelOnStart(tunnel_t *t);

void disturberTunnelUpStreamInit(tunnel_t *t, line_t *l);
void disturberTunnelUpStreamEst(tunnel_t *t, line_t *l);
void disturberTunnelUpStreamFinish(tunnel_t *t, line_t *l);
void disturberTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf);
void disturberTunnelUpStreamPause(tunnel_t *t, line_t *l);
void disturberTunnelUpStreamResume(tunnel_t *t, line_t *l);

void disturberTunnelDownStreamInit(tunnel_t *t, line_t *l);
void disturberTunnelDownStreamEst(tunnel_t *t, line_t *l);
void disturberTunnelDownStreamFinish(tunnel_t *t, line_t *l);
void disturberTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf);
void disturberTunnelDownStreamPause(tunnel_t *t, line_t *l);
void disturberTunnelDownStreamResume(tunnel_t *t, line_t *l);

void disturberLinestateInitialize(disturber_lstate_t *ls);
void disturberLinestateDestroy(disturber_lstate_t *ls);
