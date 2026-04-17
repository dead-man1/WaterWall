#include "structure.h"

#include "loggers/network_logger.h"

static bool tlsserverMaybeSendPendingEst(tunnel_t *t, line_t *l, tlsserver_lstate_t *ls)
{
    if (! ls->downstream_est_pending && lineIsEstablished(l))
    {
        return true;
    }

    if (! withLineLocked(l, tunnelPrevDownStreamEst, t))
    {
        return false;
    }

    ls->downstream_est_pending = false;
    return true;
}

static int tlsserverPerformHandshake(tunnel_t *t, line_t *l, tlsserver_lstate_t *ls)
{
    int            n      = SSL_accept(ls->ssl);
    enum sslstatus status = getSslStatus(ls->ssl, n);

    if (! tlsserverFlushSslOutput(t, l, ls))
    {
        return -1;
    }

    if (SSL_is_init_finished(ls->ssl))
    {
        ls->handshake_completed = true;

        if (! tlsserverMaybeSendPendingEst(t, l, ls))
        {
            return -1;
        }

        if (! tlsserverFlushPendingDownQueue(t, l, ls))
        {
            return -1;
        }
    }

    if (status == kSslstatusFail)
    {
        tlsserverPrintSSLError();
        return -1;
    }

    if (! ls->handshake_completed && status == kSslstatusWantIo &&
        SSL_get_error(ls->ssl, n) == SSL_ERROR_WANT_READ)
    {
        return 0;
    }

    return 1;
}

static bool tlsserverReadDecryptedData(tunnel_t *t, line_t *l, tlsserver_lstate_t *ls)
{
    while (true)
    {
        sbuf_t *data_buf = bufferpoolGetLargeBuffer(lineGetBufferPool(l));
        int     avail    = (int) sbufGetMaximumWriteableSize(data_buf);
        int     n        = SSL_read(ls->ssl, sbufGetMutablePtr(data_buf), avail);

        if (n > 0)
        {
            sbufSetLength(data_buf, n);

            if (! tlsserverFlushSslOutput(t, l, ls))
            {
                lineReuseBuffer(l, data_buf);
                return false;
            }

            if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, data_buf))
            {
                return false;
            }
            continue;
        }

        lineReuseBuffer(l, data_buf);

        if (! tlsserverFlushSslOutput(t, l, ls))
        {
            return false;
        }

        switch (SSL_get_error(ls->ssl, n))
        {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return true;
        case SSL_ERROR_ZERO_RETURN:
            ls->peer_close_notify_received = true;
            if (ls->next_finished)
            {
                return true;
            }

            ls->next_finished = true;
            return withLineLocked(l, tunnelNextUpStreamFinish, t);
        default:
            return false;
        }
    }
}

void tlsserverTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    tlsserver_lstate_t *ls = lineGetState(l, t);

    lineLock(l);

    while (sbufGetLength(buf) > 0)
    {
        int n = BIO_write(SSL_get_rbio(ls->ssl), sbufGetRawPtr(buf), (int) sbufGetLength(buf));

        if (n <= 0)
        {
            lineReuseBuffer(l, buf);
            if (lineIsAlive(l))
            {
                lineUnlock(l);
                tlsserverPrintSSLState(ls->ssl);
                tlsserverCloseLineFatal(t, l, true);
                return;
            }
            lineUnlock(l);
            return;
        }

        sbufShiftRight(buf, n);

        while (! ls->handshake_completed)
        {
            int handshake_result = tlsserverPerformHandshake(t, l, ls);

            if (handshake_result < 0)
            {
                lineReuseBuffer(l, buf);
                if (lineIsAlive(l))
                {
                    lineUnlock(l);
                    tlsserverPrintSSLState(ls->ssl);
                    tlsserverCloseLineFatal(t, l, true);
                    return;
                }
                lineUnlock(l);
                return;
            }

            if (handshake_result == 0)
            {
                break;
            }
        }

        if (ls->handshake_completed && ! tlsserverReadDecryptedData(t, l, ls))
        {
            lineReuseBuffer(l, buf);
            if (lineIsAlive(l))
            {
                lineUnlock(l);
                tlsserverPrintSSLState(ls->ssl);
                tlsserverCloseLineFatal(t, l, true);
                return;
            }
            lineUnlock(l);
            return;
        }
    }

    lineReuseBuffer(l, buf);
    lineUnlock(l);
}
