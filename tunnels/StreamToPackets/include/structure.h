#pragma once

#include "wwapi.h"

typedef struct streamtopackets_tstate_s
{
    int unused;
} streamtopackets_tstate_t;

typedef struct streamtopackets_lstate_s
{
    line_t         *line;        // Pointer to the line associated with this state
    buffer_stream_t read_stream; // Stream for reading data packets
    bool            paused;      // Indicates if the line is paused, dropping packets

} streamtopackets_lstate_t;

enum
{
    kTunnelStateSize = sizeof(streamtopackets_tstate_t),
    kLineStateSize   = sizeof(streamtopackets_lstate_t),
    kMaxBufferSize   = 65536 * 2, // Maximum buffer size for reading data packets
    kHeaderSize      = 2          // add 2 bytes to packet to store real size
};

WW_EXPORT void         streamtopacketsTunnelDestroy(tunnel_t *t);
WW_EXPORT tunnel_t    *streamtopacketsTunnelCreate(node_t *node);
WW_EXPORT api_result_t streamtopacketsTunnelApi(tunnel_t *instance, sbuf_t *message);

void streamtopacketsTunnelOnIndex(tunnel_t *t, uint16_t index, uint16_t *mem_offset);
void streamtopacketsTunnelOnChain(tunnel_t *t, tunnel_chain_t *chain);
void streamtopacketsTunnelOnPrepair(tunnel_t *t);
void streamtopacketsTunnelOnStart(tunnel_t *t);

void streamtopacketsTunnelUpStreamInit(tunnel_t *t, line_t *l);
void streamtopacketsTunnelUpStreamEst(tunnel_t *t, line_t *l);
void streamtopacketsTunnelUpStreamFinish(tunnel_t *t, line_t *l);
void streamtopacketsTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf);
void streamtopacketsTunnelUpStreamPause(tunnel_t *t, line_t *l);
void streamtopacketsTunnelUpStreamResume(tunnel_t *t, line_t *l);

void streamtopacketsTunnelDownStreamInit(tunnel_t *t, line_t *l);
void streamtopacketsTunnelDownStreamEst(tunnel_t *t, line_t *l);
void streamtopacketsTunnelDownStreamFinish(tunnel_t *t, line_t *l);
void streamtopacketsTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf);
void streamtopacketsTunnelDownStreamPause(tunnel_t *t, line_t *l);
void streamtopacketsTunnelDownStreamResume(tunnel_t *t, line_t *l);

void streamtopacketsLinestateInitialize(streamtopackets_lstate_t *ls, buffer_pool_t *pool);
void streamtopacketsLinestateDestroy(streamtopackets_lstate_t *ls);
void streamtopacketsLinestateReset(streamtopackets_lstate_t *ls);

bool streamtopacketsReadStreamIsOverflowed(buffer_stream_t *read_stream);
bool streamtopacketsTryReadIPv4Packet(buffer_stream_t *stream, sbuf_t **packet_out);
