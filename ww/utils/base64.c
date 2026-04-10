#include "base64.h"

/* BASE 64 encode table */
static const char base64en[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
};

#define BASE64_PAD      '='
#define BASE64DE_FIRST  '+'
#define BASE64DE_LAST   'z'
/* ASCII order for BASE 64 decode, -1 in unused character */
static const signed char base64de[] = {
    /* '+', ',', '-', '.', '/', '0', '1', '2', */
        62,  -1,  -1,  -1,  63,  52,  53,  54,

    /* '3', '4', '5', '6', '7', '8', '9', ':', */
        55,  56,  57,  58,  59,  60,  61,  -1,

    /* ';', '<', '=', '>', '?', '@', 'A', 'B', */
        -1,  -1,  -1,  -1,  -1,  -1,   0,   1,

    /* 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', */
         2,   3,   4,   5,   6,   7,   8,   9,

    /* 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', */
        10,  11,  12,  13,  14,  15,  16,  17,

    /* 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', */
        18,  19,  20,  21,  22,  23,  24,  25,

    /* '[', '\', ']', '^', '_', '`', 'a', 'b', */
        -1,  -1,  -1,  -1,  -1,  -1,  26,  27,

    /* 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', */
        28,  29,  30,  31,  32,  33,  34,  35,

    /* 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', */
        36,  37,  38,  39,  40,  41,  42,  43,

    /* 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', */
        44,  45,  46,  47,  48,  49,  50,  51,
};

int wwBase64Encode(const unsigned char *in, unsigned int inlen, char *out) {
    unsigned int i = 0, j = 0;

    if (inlen == 0) {
        return 0;
    }

    for (; i < inlen; i++) {
        int s = (int) (i % 3);

        switch (s) {
        case 0:
            out[j++] = base64en[(in[i] >> 2) & 0x3F];
            continue;
        case 1:
            out[j++] = base64en[((in[i-1] & 0x3) << 4) + ((in[i] >> 4) & 0xF)];
            continue;
        case 2:
            out[j++] = base64en[((in[i-1] & 0xF) << 2) + ((in[i] >> 6) & 0x3)];
            out[j++] = base64en[in[i] & 0x3F];
        }
    }

    /* move back */
    i -= 1;

    /* check the last and add padding */
    if ((i % 3) == 0) {
        out[j++] = base64en[(in[i] & 0x3) << 4];
        out[j++] = BASE64_PAD;
        out[j++] = BASE64_PAD;
    } else if ((i % 3) == 1) {
        out[j++] = base64en[(in[i] & 0xF) << 2];
        out[j++] = BASE64_PAD;
    }

    return (int) (j);
}

int wwBase64Decode(const char *in, unsigned int inlen, unsigned char *out) {
    unsigned int i = 0, j = 0;

    if ((inlen % 4) != 0) {
        return -1;
    }

    for (; i < inlen; i += 4) {
        int c0, c1, c2, c3;
        unsigned char ch0 = (unsigned char)in[i];
        unsigned char ch1 = (unsigned char)in[i + 1];
        unsigned char ch2 = (unsigned char)in[i + 2];
        unsigned char ch3 = (unsigned char)in[i + 3];

        if (ch0 < BASE64DE_FIRST || ch0 > BASE64DE_LAST ||
            (c0 = base64de[ch0 - BASE64DE_FIRST]) == -1) {
            return -1;
        }
        if (ch1 < BASE64DE_FIRST || ch1 > BASE64DE_LAST ||
            (c1 = base64de[ch1 - BASE64DE_FIRST]) == -1) {
            return -1;
        }

        if (ch2 == BASE64_PAD) {
            /* xx== is only valid in the final 4-char block */
            if (ch3 != BASE64_PAD || (i + 4) != inlen) {
                return -1;
            }
            out[j++] = (unsigned char)(((unsigned int)c0 << 2) | (((unsigned int)c1 >> 4) & 0x3));
            return (int)j;
        }

        if (ch2 < BASE64DE_FIRST || ch2 > BASE64DE_LAST ||
            (c2 = base64de[ch2 - BASE64DE_FIRST]) == -1) {
            return -1;
        }

        if (ch3 == BASE64_PAD) {
            /* xxx= is only valid in the final 4-char block */
            if ((i + 4) != inlen) {
                return -1;
            }
            out[j++] = (unsigned char)(((unsigned int)c0 << 2) | (((unsigned int)c1 >> 4) & 0x3));
            out[j++] = (unsigned char)((((unsigned int)c1 & 0xF) << 4) | (((unsigned int)c2 >> 2) & 0xF));
            return (int)j;
        }

        if (ch3 < BASE64DE_FIRST || ch3 > BASE64DE_LAST ||
            (c3 = base64de[ch3 - BASE64DE_FIRST]) == -1) {
            return -1;
        }

        out[j++] = (unsigned char)(((unsigned int)c0 << 2) | (((unsigned int)c1 >> 4) & 0x3));
        out[j++] = (unsigned char)((((unsigned int)c1 & 0xF) << 4) | (((unsigned int)c2 >> 2) & 0xF));
        out[j++] = (unsigned char)((((unsigned int)c2 & 0x3) << 6) | (unsigned int)c3);
    }

    return (int)(j);

}
