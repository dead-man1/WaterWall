#include "structure.h"

#include "loggers/network_logger.h"

void pingserverUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    pingserverDecapsulatePacket(t, l, buf);
}
