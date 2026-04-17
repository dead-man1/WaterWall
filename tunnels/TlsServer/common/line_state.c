#include "structure.h"

#include "loggers/network_logger.h"

bool tlsserverLinestateInitialize(tlsserver_lstate_t *ls, SSL_CTX *ssl_ctx)
{
    *ls = (tlsserver_lstate_t) {.pending_down = bufferqueueCreate(2)};

    ls->rbio = BIO_new(BIO_s_mem());
    ls->wbio = BIO_new(BIO_s_mem());
    ls->ssl  = SSL_new(ssl_ctx);

    if (ls->rbio == NULL || ls->wbio == NULL || ls->ssl == NULL)
    {
        tlsserverLinestateDestroy(ls);
        return false;
    }

    SSL_set_accept_state(ls->ssl);
    SSL_set_bio(ls->ssl, ls->rbio, ls->wbio);
    ls->rbio = NULL;
    ls->wbio = NULL;

    return true;
}

void tlsserverLinestateRelease(tlsserver_lstate_t *ls)
{
    if (ls->resources_released)
    {
        return;
    }
    ls->resources_released = true;

    if (ls->ssl != NULL)
    {
        SSL_free(ls->ssl);
        ls->ssl = NULL;
    }
    else
    {
        if (ls->rbio != NULL)
        {
            BIO_free(ls->rbio);
            ls->rbio = NULL;
        }
        if (ls->wbio != NULL)
        {
            BIO_free(ls->wbio);
            ls->wbio = NULL;
        }
    }

    bufferqueueDestroy(&ls->pending_down);

    ls->handshake_completed    = false;
    ls->downstream_est_pending = false;
}

void tlsserverLinestateDestroy(tlsserver_lstate_t *ls)
{
    tlsserverLinestateRelease(ls);
    memoryZeroAligned32(ls, sizeof(tlsserver_lstate_t));
}
