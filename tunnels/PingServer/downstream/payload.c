#include "structure.h"

#include "loggers/network_logger.h"

void pingserverDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingserverEncapsulatePacket(t, l, buf);
}
