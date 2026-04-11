#include "structure.h"

#include "loggers/network_logger.h"

static void closeLineBidirectional(tunnel_t *t, line_t *l)
{
    encryptionserver_lstate_t *ls = lineGetState(l, t);

    if (! ls->next_finished)
    {
        ls->next_finished = true;
        if (! withLineLocked(l, tunnelNextUpStreamFinish, t))
        {
            return;
        }
    }

    if (! ls->prev_finished)
    {
        ls->prev_finished = true;
        withLineLocked(l, tunnelPrevDownStreamFinish, t);
    }
}

void encryptionserverTunnelDownStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    encryptionserver_tstate_t *ts            = tunnelGetState(t);
    encryptionserver_lstate_t *ls            = lineGetState(l, t);
    uint32_t                   plaintext_len = sbufGetLength(buf);

    if (plaintext_len == 0)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    if (plaintext_len > ts->max_frame_payload)
    {
        LOGW("EncryptionServer: plaintext length exceeds configured max-frame-size: %u > %u, dropped", plaintext_len,
             ts->max_frame_payload);
        lineReuseBuffer(l, buf);
        return;
    }

    uint32_t ciphertext_len = plaintext_len + kEncryptionTagSize;
    uint32_t frame_len      = kEncryptionFramePrefixSize + ciphertext_len;
    uint32_t tail_growth    = ciphertext_len - plaintext_len;

    if (sbufGetMaximumWriteableSize(buf) < tail_growth)
    {
        buf = sbufReserveSpace(buf, tail_growth);
    }

    if (sbufGetLeftCapacity(buf) < kEncryptionFramePrefixSize)
    {
        sbuf_t *framed = sbufCreateWithPadding(plaintext_len + tail_growth, kEncryptionFramePrefixSize);
        sbufSetLength(framed, plaintext_len);
        sbufWriteBuf(framed, buf, plaintext_len);
        lineReuseBuffer(l, buf);
        buf = framed;
    }

    sbufShiftLeft(buf, kEncryptionFramePrefixSize);

    uint8_t *frame = sbufGetMutablePtr(buf);
    frame[0]       = kEncryptionFrameMagic0;
    frame[1]       = kEncryptionFrameMagic1;
    frame[2]       = kEncryptionFrameVersion;
    frame[3]       = (uint8_t) ts->algorithm;

    uint32_t ciphertext_len_be = htobe32(ciphertext_len);
    memoryCopy(frame + 4, &ciphertext_len_be, sizeof(ciphertext_len_be));

    uint8_t *nonce = frame + kEncryptionFrameHeaderSize;
    getRandomBytes(nonce, kEncryptionNonceSize);

    uint8_t *ciphertext = frame + kEncryptionFramePrefixSize;

    if (0 != encryptionserverEncryptAead(ts->algorithm, ciphertext, ciphertext, plaintext_len, frame,
                                         kEncryptionFramePrefixSize, nonce, ts->key))
    {
        LOGW("EncryptionServer: failed to encrypt payload, closing line");
        lineReuseBuffer(l, buf);
        closeLineBidirectional(t, l);
        return;
    }

    sbufSetLength(buf, frame_len);
    if (! withLineLockedWithBuf(l, tunnelPrevDownStreamPayload, t, buf))
    {
        ls->prev_finished = true;
    }
}
