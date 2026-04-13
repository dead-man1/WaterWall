#pragma once

#include "wwapi.h"

typedef struct packetstostream_tstate_s
{
    int unused;
} packetstostream_tstate_t;

typedef struct packetstostream_lstate_s
{
    line_t         *line;        // Pointer to the line associated with this state
    buffer_stream_t read_stream; // Stream for reading data packets
    bool            paused;      // Indicates if the line is paused, dropping packets
    bool            recreate_scheduled;

} packetstostream_lstate_t;

enum
{
    kTunnelStateSize = sizeof(packetstostream_tstate_t),
    kLineStateSize   = sizeof(packetstostream_lstate_t),
    kMaxBufferSize   = 65536 * 2, // Maximum buffer size for reading data packets
    kHeaderSize      = 2          // add 2 bytes to packet to store real size
};

WW_EXPORT void         packetstostreamTunnelDestroy(tunnel_t *t);
WW_EXPORT tunnel_t    *packetstostreamTunnelCreate(node_t *node);
WW_EXPORT api_result_t packetstostreamTunnelApi(tunnel_t *instance, sbuf_t *message);

void packetstostreamTunnelOnIndex(tunnel_t *t, uint16_t index, uint16_t *mem_offset);
void packetstostreamTunnelOnChain(tunnel_t *t, tunnel_chain_t *chain);
void packetstostreamTunnelOnPrepair(tunnel_t *t);
void packetstostreamTunnelOnStart(tunnel_t *t);

void packetstostreamTunnelUpStreamInit(tunnel_t *t, line_t *l);
void packetstostreamTunnelUpStreamEst(tunnel_t *t, line_t *l);
void packetstostreamTunnelUpStreamFinish(tunnel_t *t, line_t *l);
void packetstostreamTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf);
void packetstostreamTunnelUpStreamPause(tunnel_t *t, line_t *l);
void packetstostreamTunnelUpStreamResume(tunnel_t *t, line_t *l);

void packetstostreamTunnelDownStreamInit(tunnel_t *t, line_t *l);
void packetstostreamTunnelDownStreamEst(tunnel_t *t, line_t *l);
void packetstostreamTunnelDownStreamFinish(tunnel_t *t, line_t *l);
void packetstostreamTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf);
void packetstostreamTunnelDownStreamPause(tunnel_t *t, line_t *l);
void packetstostreamTunnelDownStreamResume(tunnel_t *t, line_t *l);

void packetstostreamLinestateInitialize(packetstostream_lstate_t *ls, buffer_pool_t *pool);
void packetstostreamLinestateDestroy(packetstostream_lstate_t *ls);

bool    packetstostreamReadStreamIsOverflowed(buffer_stream_t *read_stream);
bool    packetstostreamTryReadIPv4Packet(buffer_stream_t *stream, sbuf_t **packet_out);
void    packetstostreamRecreateOutputLineTask(tunnel_t *t, line_t *packet_line);
line_t *packetstostreamEnsureOutputLine(tunnel_t *t, line_t *packet_line, packetstostream_lstate_t *ls);
