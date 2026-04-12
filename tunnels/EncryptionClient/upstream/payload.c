#include "structure.h"

#include "loggers/network_logger.h"


void encryptionclientTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    encryptionclient_tstate_t *ts            = tunnelGetState(t);
    encryptionclient_lstate_t *ls            = lineGetState(l, t);
    uint32_t                   plaintext_len = sbufGetLength(buf);

    if (plaintext_len == 0)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    if (plaintext_len > ts->max_frame_payload)
    {
        LOGW("EncryptionClient: plaintext length exceeds configured max-frame-size: %u > %u, dropped", plaintext_len,
             ts->max_frame_payload);
        lineReuseBuffer(l, buf);
        return;
    }

    uint32_t ciphertext_len = plaintext_len + kEncryptionTagSize;
    uint32_t frame_len      = kEncryptionFramePrefixSize + ciphertext_len;

    if (sbufGetMaximumWriteableSize(buf) < ciphertext_len)
    {
        buf = sbufReserveSpace(buf, ciphertext_len);
    }

    assert(sbufGetLeftCapacity(buf) >= kEncryptionFramePrefixSize);

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

    if (0 != encryptionclientEncryptAead(ts->algorithm, ciphertext, ciphertext, plaintext_len, frame,
                                         kEncryptionFramePrefixSize, nonce, ts->key))
    {
        LOGW("EncryptionClient: failed to encrypt payload, closing line");
        lineReuseBuffer(l, buf);
        encryptionclientCloseLineBidirectional(t, l);
        return;
    }

    sbufSetLength(buf, frame_len);
    if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, buf))
    {
        ls->next_finished = true;
    }
}
