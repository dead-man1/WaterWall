#include "structure.h"

#include "loggers/network_logger.h"

enum frame_read_result_e
{
    kFrameReadNeedMore = 0,
    kFrameReadOk       = 1,
    kFrameReadInvalid  = -1,
};



static int tryReadCompleteFrame(buffer_stream_t *stream, const encryptionserver_tstate_t *ts, sbuf_t **frame_buffer)
{
    if (bufferstreamGetBufLen(stream) < kEncryptionFramePrefixSize)
    {
        return kFrameReadNeedMore;
    }

    uint8_t prefix[kEncryptionFramePrefixSize];
    bufferstreamViewBytesAt(stream, 0, prefix, sizeof(prefix));

    if (prefix[0] != kEncryptionFrameMagic0 || prefix[1] != kEncryptionFrameMagic1 ||
        prefix[2] != kEncryptionFrameVersion || prefix[3] != (uint8_t) ts->algorithm)
    {
        return kFrameReadInvalid;
    }

    uint32_t ciphertext_len_be;
    memoryCopy(&ciphertext_len_be, prefix + 4, sizeof(ciphertext_len_be));

    uint32_t ciphertext_len = be32toh(ciphertext_len_be);
    if (ciphertext_len < kEncryptionTagSize || ciphertext_len > ts->max_frame_payload + kEncryptionTagSize)
    {
        return kFrameReadInvalid;
    }

    uint32_t frame_len = kEncryptionFramePrefixSize + ciphertext_len;
    if (frame_len > bufferstreamGetBufLen(stream))
    {
        return kFrameReadNeedMore;
    }

    *frame_buffer = bufferstreamReadExact(stream, frame_len);
    return kFrameReadOk;
}

static bool isOverflow(buffer_stream_t *read_stream, const encryptionserver_tstate_t *ts)
{
    size_t max_frame = (size_t) ts->max_frame_payload + kEncryptionTagSize + kEncryptionFramePrefixSize;
    size_t limit     = max_frame * 2;

    if (bufferstreamGetBufLen(read_stream) > limit)
    {
        LOGW("EncryptionServer: UpStreamPayload: read stream overflow, size: %zu, limit: %zu",
             bufferstreamGetBufLen(read_stream), limit);
        return true;
    }
    return false;
}

void encryptionserverTunnelUpStreamPayload(tunnel_t *t, line_t *l, sbuf_t *buf)
{
    encryptionserver_tstate_t *ts = tunnelGetState(t);
    encryptionserver_lstate_t *ls = lineGetState(l, t);

    bufferstreamPush(&(ls->read_stream), buf);

    if (isOverflow(&(ls->read_stream), ts))
    {
        bufferstreamEmpty(&(ls->read_stream));
        encryptionserverCloseLineBidirectional(t, l);
        return;
    }

    while (true)
    {
        sbuf_t *frame_buffer = NULL;
        int     read_result  = tryReadCompleteFrame(&(ls->read_stream), ts, &frame_buffer);

        if (read_result == kFrameReadNeedMore)
        {
            break;
        }

        if (read_result == kFrameReadInvalid)
        {
            LOGW("EncryptionServer: invalid encrypted frame received, closing line");
            bufferstreamEmpty(&(ls->read_stream));
            encryptionserverCloseLineBidirectional(t, l);
            return;
        }

        uint8_t *frame = sbufGetMutablePtr(frame_buffer);

        uint32_t ciphertext_len_be;
        memoryCopy(&ciphertext_len_be, frame + 4, sizeof(ciphertext_len_be));
        uint32_t ciphertext_len = be32toh(ciphertext_len_be);

        uint8_t *nonce      = frame + kEncryptionFrameHeaderSize;
        uint8_t *ciphertext = frame + kEncryptionFramePrefixSize;

        if (0 != encryptionserverDecryptAead(ts->algorithm, ciphertext, ciphertext, ciphertext_len, frame,
                                             kEncryptionFramePrefixSize, nonce, ts->key))
        {
            LOGW("EncryptionServer: failed to decrypt frame, closing line");
            lineReuseBuffer(l, frame_buffer);
            bufferstreamEmpty(&(ls->read_stream));
            encryptionserverCloseLineBidirectional(t, l);
            return;
        }

        sbufShiftRight(frame_buffer, kEncryptionFramePrefixSize);
        sbufSetLength(frame_buffer, ciphertext_len - kEncryptionTagSize);

        if (! withLineLockedWithBuf(l, tunnelNextUpStreamPayload, t, frame_buffer))
        {
            return;
        }
    }
}
