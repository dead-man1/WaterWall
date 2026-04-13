#include "structure.h"

#include "loggers/network_logger.h"

void packetstostreamTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    packetstostream_lstate_t *ls = lineGetState(l, t);
    line_t                   *stream_line;

    stream_line = packetstostreamEnsureOutputLine(t, l, ls);

    if (stream_line == NULL || ls->paused || sbufGetLength(buf) > kMaxAllowedPacketLength)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    struct ip_hdr *ipheader = (struct ip_hdr *) sbufGetMutablePtr(buf);

    if (l->recalculate_checksum && IPH_V(ipheader) == 4)
    {
        calcFullPacketChecksum(sbufGetMutablePtr(buf));
        l->recalculate_checksum = false;
    }

    uint32_t packet_length = sbufGetLength(buf);

    // safely cast to uint16_t, since kMaxAllowedPacketLength is lower than 65536
    uint16_t packet_length_network = htons(packet_length);

    sbufShiftLeft(buf, sizeof(uint16_t));
    // cant gurantee the alignment of the buffer, so we use unaligned write
    sbufWriteUnAlignedUI16(buf, packet_length_network);

    tunnelNextUpStreamPayload(t, stream_line, buf);
}
