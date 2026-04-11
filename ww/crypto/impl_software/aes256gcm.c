#include "wcrypto.h"

int aes256gcmIsAvailable(void)
{
    return 0;
}

int aes256gcmEncrypt(unsigned char *dst, const unsigned char *src, size_t src_len, const unsigned char *ad,
                     size_t ad_len, const unsigned char *nonce, const unsigned char *key)
{
    discard dst;
    discard src;
    discard src_len;
    discard ad;
    discard ad_len;
    discard nonce;
    discard key;
    return -1;
}

int aes256gcmDecrypt(unsigned char *dst, const unsigned char *src, size_t src_len, const unsigned char *ad,
                     size_t ad_len, const unsigned char *nonce, const unsigned char *key)
{
    discard dst;
    discard src;
    discard src_len;
    discard ad;
    discard ad_len;
    discard nonce;
    discard key;
    return -1;
}
