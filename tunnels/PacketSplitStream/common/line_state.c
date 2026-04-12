#include "structure.h"

void packetsplitstreamLinestateInitializePacket(packetsplitstream_lstate_t *ls, line_t *packet_line)
{
    *ls = (packetsplitstream_lstate_t) {.packet_line   = packet_line,
                                        .upload_line   = NULL,
                                        .download_line = NULL,
                                        .role          = kPacketSplitStreamRolePacket};
}

void packetsplitstreamLinestateInitializeChild(packetsplitstream_lstate_t *ls, line_t *packet_line,
                                               packetsplitstream_role_t role)
{
    *ls = (packetsplitstream_lstate_t) {.packet_line   = packet_line,
                                        .upload_line   = NULL,
                                        .download_line = NULL,
                                        .role          = role};
}

void packetsplitstreamLinestateDestroy(packetsplitstream_lstate_t *ls)
{
    memoryZeroAligned32(ls, sizeof(packetsplitstream_lstate_t));
}
