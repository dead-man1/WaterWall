#include "structure.h"

#include "loggers/network_logger.h"

static sbuf_t *encryptionclientAllocFrameBuffer(buffer_pool_t *pool, uint32_t ciphertext_len)
{
    uint32_t small_size = bufferpoolGetSmallBufferSize(pool);
    uint32_t large_size = bufferpoolGetLargeBufferSize(pool);

    if (ciphertext_len <= small_size)
    {
        return bufferpoolGetSmallBuffer(pool);
    }

    if (ciphertext_len <= large_size)
    {
        return bufferpoolGetLargeBuffer(pool);
    }

    return sbufCreateWithPadding(ciphertext_len, bufferpoolGetLargeBufferPadding(pool));
}

static bool encryptionclientEncryptFrame(encryptionclient_tstate_t *ts, sbuf_t **buf, uint32_t plaintext_len)
{
    uint32_t ciphertext_len = plaintext_len + kEncryptionTagSize;
    uint32_t frame_len      = kEncryptionFramePrefixSize + ciphertext_len;

    if (sbufGetMaximumWriteableSize(*buf) < ciphertext_len)
    {
        *buf = sbufReserveSpace(*buf, ciphertext_len);
    }

    assert(sbufGetLeftCapacity(*buf) >= kEncryptionFramePrefixSize);

    sbufShiftLeft(*buf, kEncryptionFramePrefixSize);

    uint8_t *frame = sbufGetMutablePtr(*buf);
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
        return false;
    }

    sbufSetLength(*buf, frame_len);
    return true;
}

void encryptionclientTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    encryptionclient_tstate_t *ts            = tunnelGetState(t);
    uint32_t                   plaintext_len = sbufGetLength(buf);

    if (plaintext_len == 0)
    {
        lineReuseBuffer(l, buf);
        return;
    }

    if (plaintext_len > ts->max_frame_payload)
    {
        buffer_pool_t *pool      = lineGetBufferPool(l);
        const uint8_t *src       = sbufGetRawPtr(buf);
        uint32_t       remaining = plaintext_len;

        while (remaining > 0)
        {
            uint32_t chunk_len      = min(remaining, ts->max_frame_payload);
            uint32_t ciphertext_len = chunk_len + kEncryptionTagSize;
            sbuf_t  *frame_buf      = encryptionclientAllocFrameBuffer(pool, ciphertext_len);

            sbufSetLength(frame_buf, chunk_len);
            memoryCopyLarge(sbufGetMutablePtr(frame_buf), src, chunk_len);

            if (! encryptionclientEncryptFrame(ts, &frame_buf, chunk_len))
            {
                LOGW("EncryptionClient: failed to encrypt payload chunk, closing line");
                bufferpoolReuseBuffer(pool, frame_buf);
                bufferpoolReuseBuffer(pool, buf);
                encryptionclientCloseLineBidirectional(t, l);
                return;
            }

            src += chunk_len;
            remaining -= chunk_len;

            if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, frame_buf))
            {
                bufferpoolReuseBuffer(pool, buf);
                return;
            }
        }

        lineReuseBuffer(l, buf);
        return;
    }

    if (! encryptionclientEncryptFrame(ts, &buf, plaintext_len))
    {
        LOGW("EncryptionClient: failed to encrypt payload, closing line");
        lineReuseBuffer(l, buf);
        encryptionclientCloseLineBidirectional(t, l);
        return;
    }

    if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, buf))
    {
        return;
    }
}
