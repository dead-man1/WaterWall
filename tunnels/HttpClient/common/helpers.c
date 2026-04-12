#include "structure.h"

#include "loggers/network_logger.h"

static inline bool asciiCaseEqual(char a, char b)
{
    return (char) tolower((unsigned char) a) == (char) tolower((unsigned char) b);
}

static inline char *httpclientNextToken(char *str, const char *delim, char **saveptr)
{
#ifdef COMPILER_MSVC
    return strtok_s(str, delim, saveptr);
#else
    return strtok_r(str, delim, saveptr);
#endif
}

bool httpclientStringCaseEquals(const char *a, const char *b)
{
    if (a == NULL || b == NULL)
    {
        return false;
    }

    while (*a != '\0' && *b != '\0')
    {
        if (! asciiCaseEqual(*a, *b))
        {
            return false;
        }
        ++a;
        ++b;
    }

    return (*a == '\0' && *b == '\0');
}

bool httpclientStringCaseContains(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL || *needle == '\0')
    {
        return false;
    }

    size_t needle_len = strlen(needle);
    size_t hay_len    = strlen(haystack);

    if (needle_len > hay_len)
    {
        return false;
    }

    for (size_t i = 0; i <= hay_len - needle_len; i++)
    {
        bool match = true;
        for (size_t j = 0; j < needle_len; j++)
        {
            if (! asciiCaseEqual(haystack[i + j], needle[j]))
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            return true;
        }
    }

    return false;
}

bool httpclientStringCaseContainsToken(const char *value, const char *token)
{
    if (value == NULL || token == NULL)
    {
        return false;
    }

    char *tmp = stringDuplicate(value);
    stringLowerCase(tmp);

    char *token_l = stringDuplicate(token);
    stringLowerCase(token_l);

    bool  found   = false;
    char *saveptr = NULL;

    for (char *part = httpclientNextToken(tmp, ",", &saveptr); part != NULL;
         part = httpclientNextToken(NULL, ",", &saveptr))
    {
        while (*part == ' ' || *part == '\t')
        {
            ++part;
        }

        char *end = part + strlen(part);
        while (end > part && (end[-1] == ' ' || end[-1] == '\t'))
        {
            --end;
        }
        *end = '\0';

        if (stringCompare(part, token_l) == 0)
        {
            found = true;
            break;
        }
    }

    memoryFree(token_l);
    memoryFree(tmp);

    return found;
}

bool bufferstreamFindCRLF(buffer_stream_t *stream, size_t *line_end)
{
    size_t len = bufferstreamGetBufLen(stream);

    if (len < 2)
    {
        return false;
    }

    for (size_t i = 0; i + 1 < len; i++)
    {
        uint8_t c1 = bufferstreamViewByteAt(stream, i);
        uint8_t c2 = bufferstreamViewByteAt(stream, i + 1);

        if (c1 == '\r' && c2 == '\n')
        {
            *line_end = i;
            return true;
        }
    }

    return false;
}

bool bufferstreamFindDoubleCRLF(buffer_stream_t *stream, size_t *header_end)
{
    size_t len = bufferstreamGetBufLen(stream);

    if (len < 4)
    {
        return false;
    }

    for (size_t i = 0; i + 3 < len; i++)
    {
        uint8_t b0 = bufferstreamViewByteAt(stream, i + 0);
        uint8_t b1 = bufferstreamViewByteAt(stream, i + 1);
        uint8_t b2 = bufferstreamViewByteAt(stream, i + 2);
        uint8_t b3 = bufferstreamViewByteAt(stream, i + 3);

        if (b0 == '\r' && b1 == '\n' && b2 == '\r' && b3 == '\n')
        {
            *header_end = i + 4;
            return true;
        }
    }

    return false;
}


sbuf_t *allocBufferForLength(line_t *l, uint32_t len)
{
    buffer_pool_t *pool = lineGetBufferPool(l);
    uint32_t       small_size = bufferpoolGetSmallBufferSize(pool);
    uint32_t       large_size = bufferpoolGetLargeBufferSize(pool);

    if (len <= small_size)
    {
        return bufferpoolGetSmallBuffer(pool);
    }

    if (len <= large_size)
    {
        return bufferpoolGetLargeBuffer(pool);
    }

    return sbufCreateWithPadding(len, bufferpoolGetLargeBufferPadding(pool));
}
