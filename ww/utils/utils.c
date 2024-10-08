#include "basic_types.h"
#include "cJSON.h"
#include "fileutils.h"
#include "hashutils.h"
#include "hlog.h"
#include "hplatform.h"
#include "jsonutils.h"
#include "managers/memory_manager.h"
#include "procutils.h"
#include "sockutils.h"
#include "stringutils.h"
#include "userutils.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define NOMINMAX
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char *readFile(const char *const path)
{
    FILE *f = fopen(path, "rb");

    if (! f)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET); /* same as rewind(f); */

    char  *string = globalMalloc(fsize + 1);
    size_t count  = fread(string, fsize, 1, f);
    if (count == 0)
    {
        globalFree(string);
        return NULL;
    }
    fclose(f);

    string[fsize] = 0;
    return string;
}

bool writeFile(const char *const path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");

    if (! f)
    {
        return false;
    }

    fseek(f, 0, SEEK_SET);

    if (fwrite(data, len, 1, f) != len)
    {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

char *concat(const char *s1, const char *s2)
{
    char *result = globalMalloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void toUpperCase(char *str)
{
    int i = 0;
    while (str[i] != 0x0)
    {
        str[i] = (char) toupper(str[i]);
        i++;
    }
}

void toLowerCase(char *str)
{
    int i = 0;
    while (str[i] != 0x0)
    {
        str[i] = (char) tolower(str[i]);
        i++;
    }
}

bool getBoolFromJsonObject(bool *dest, const cJSON *json_obj, const char *key)
{
    assert(dest != NULL);
    const cJSON *jbool = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsBool(jbool))
    {
        *dest = cJSON_IsTrue(jbool);
        return true;
    }
    return false;
}

bool getBoolFromJsonObjectOrDefault(bool *dest, const cJSON *json_obj, const char *key, bool def)
{
    assert(dest != NULL);
    const cJSON *jbool = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsBool(jbool))
    {
        *dest = cJSON_IsTrue(jbool);
        return true;
    }
    *dest = def;
    return false;
}

bool getIntFromJsonObject(int *dest, const cJSON *json_obj, const char *key)
{
    assert(dest != NULL);
    const cJSON *jnumber = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsNumber(jnumber))
    {
        *dest = jnumber->valueint;
        return true;
    }
    return false;
}

bool getIntFromJsonObjectOrDefault(int *dest, const cJSON *json_obj, const char *key, int def)
{
    assert(dest != NULL);
    const cJSON *jnumber = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsNumber(jnumber))
    {
        *dest = jnumber->valueint;
        return true;
    }
    *dest = def;
    return false;
}

bool getStringFromJson(char **dest, const cJSON *json_str_node)
{
    assert(*dest == NULL);
    if (cJSON_IsString(json_str_node) && (json_str_node->valuestring != NULL))
    {

        *dest = globalMalloc(strlen(json_str_node->valuestring) + 1);
        strcpy(*dest, json_str_node->valuestring);
        return true;
    }
    return false;
}

bool getStringFromJsonObject(char **dest, const cJSON *json_obj, const char *key)
{

    assert(*dest == NULL);
    const cJSON *jstr = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsString(jstr) && (jstr->valuestring != NULL))
    {

        *dest = globalMalloc(strlen(jstr->valuestring) + 1);
        strcpy(*dest, jstr->valuestring);
        return true;
    }
    return false;
}

bool getStringFromJsonObjectOrDefault(char **dest, const cJSON *json_obj, const char *key, const char *def) // NOLINT
{
    assert(def != NULL);
    if (! getStringFromJsonObject(dest, json_obj, key))
    {
        *dest = globalMalloc(strlen(def) + 1);
        strcpy(*dest, def);
        return false;
    }
    return true;
}

void sockAddrCopy(sockaddr_u *restrict dest, const sockaddr_u *restrict source)
{
    if (source->sa.sa_family == AF_INET)
    {
        memcpy(&(dest->sin.sin_addr.s_addr), &(source->sin.sin_addr.s_addr), sizeof(source->sin.sin_addr.s_addr));
        return;
    }
    memcpy(&(dest->sin6.sin6_addr.s6_addr), &(source->sin6.sin6_addr.s6_addr), sizeof(source->sin6.sin6_addr.s6_addr));
}

bool sockAddrCmpIPV4(const sockaddr_u *restrict addr1, const sockaddr_u *restrict addr2)
{
    return (addr1->sin.sin_addr.s_addr == addr2->sin.sin_addr.s_addr);
}

bool sockAddrCmpIPV6(const sockaddr_u *restrict addr1, const sockaddr_u *restrict addr2)
{
    int r = memcmp(addr1->sin6.sin6_addr.s6_addr, addr2->sin6.sin6_addr.s6_addr, sizeof(addr1->sin6.sin6_addr.s6_addr));
    if (r != 0)
    {
        return false;
    }
    if (addr1->sin6.sin6_flowinfo != addr2->sin6.sin6_flowinfo)
    {
        return false;
    }
    if (addr1->sin6.sin6_scope_id != addr2->sin6.sin6_scope_id)
    {
        return false;
    }
    return true;
}

bool sockAddrCmpIP(const sockaddr_u *restrict addr1, const sockaddr_u *restrict addr2)
{

    if (addr1->sa.sa_family != addr2->sa.sa_family)
    {
        return false;
    }
    if (addr1->sa.sa_family == AF_INET)
    {
        return sockAddrCmpIPV4(addr1, addr2);
    }

    if (addr1->sa.sa_family == AF_INET6)
    {
        return sockAddrCmpIPV6(addr1, addr2);
    }

    assert(! "unknown sa_family");

    return false;
}

void socketContextAddrCopy(socket_context_t *dest, const socket_context_t *const source)
{
    dest->address_protocol = source->address_protocol;
    dest->address_type     = source->address_type;
    switch (dest->address_type)
    {
    case kSatIPV4:
        dest->address.sa.sa_family = AF_INET;
        dest->address.sin.sin_addr = source->address.sin.sin_addr;

        break;

    case kSatDomainName:
        dest->address.sa.sa_family = AF_INET;
        if (source->domain != NULL)
        {
            if (source->domain_constant)
            {
                socketContextDomainSetConstMem(dest, source->domain, source->domain_len);
            }
            else
            {
                socketContextDomainSet(dest, source->domain, source->domain_len);
            }
            dest->domain_resolved = source->domain_resolved;
            if (source->domain_resolved)
            {
                dest->domain_resolved = true;
                sockAddrCopy(&(dest->address), &(source->address));
            }
        }

        break;

    case kSatIPV6:
        dest->address.sa.sa_family = AF_INET6;
        memcpy(&(dest->address.sin6.sin6_addr), &(source->address.sin6.sin6_addr), sizeof(struct in6_addr));

        break;
    }
}

void socketContextPortCopy(socket_context_t *dest, socket_context_t *source)
{
    // this is supposed to work for both ipv4/6
    dest->address.sin.sin_port = source->address.sin.sin_port;

    // alternative:

    // switch (dest->address_type)
    // {
    // case kSatIPV4:
    // case kSatDomainName:
    // default:
    //     dest->address.sin.sin_port = source->address.sin.sin_port;
    //     break;

    // case kSatIPV6:
    //     dest->address.sin6.sin6_port = source->address.sin6.sin6_port;
    //     break;
    // }
}

void socketContextPortSet(socket_context_t *dest, uint16_t port)
{
    dest->address.sin.sin_port = htons(port);
}

void socketContextDomainSet(socket_context_t *restrict scontext, const char *restrict domain, uint8_t len)
{
    if (scontext->domain != NULL)
    {
        if (scontext->domain_constant)
        {
            scontext->domain = globalMalloc(256);
        }
    }
    else
    {
        scontext->domain = globalMalloc(256);
    }
    scontext->domain_constant = false;
    memcpy(scontext->domain, domain, len);
    scontext->domain[len] = 0x0;
    scontext->domain_len  = len;
}

void socketContextDomainSetConstMem(socket_context_t *restrict scontext, const char *restrict domain, uint8_t len)
{
    if (scontext->domain != NULL && ! scontext->domain_constant)
    {
        globalFree(scontext->domain);
    }
    scontext->domain_constant = true;
    scontext->domain          = (char *) domain;
    scontext->domain_len      = len;
    assert(scontext->domain[len] == 0x0);
}

hash_t sockAddrCalcHashNoPort(const sockaddr_u *saddr)
{
    hash_t result;
    if (saddr->sa.sa_family == AF_INET)
    {
        result = CALC_HASH_BYTES(&(saddr->sin.sin_addr), sizeof(struct sockaddr_in));
    }
    else if (saddr->sa.sa_family == AF_INET6)
    {
        result = CALC_HASH_BYTES(&(saddr->sin6.sin6_addr), sizeof(struct sockaddr_in6));
    }
    else
    {
        assert(false);
        perror("sockAddrCalcHashNoPort");
        exit(1);
    }
    return result;
}

hash_t sockAddrCalcHashWithPort(const sockaddr_u *saddr)
{
    hash_t result;
    if (saddr->sa.sa_family == AF_INET)
    {
        result = CALC_HASH_PRIMITIVE(saddr->sin.sin_port);
        result = CALC_HASH_BYTES_WITH_SEED(&(saddr->sin.sin_addr), sizeof(struct sockaddr_in), result);
    }
    else if (saddr->sa.sa_family == AF_INET6)
    {
        result = CALC_HASH_PRIMITIVE(saddr->sin6.sin6_port);
        result = CALC_HASH_BYTES_WITH_SEED(&(saddr->sin6.sin6_addr), sizeof(struct sockaddr_in6), result);
    }
    else
    {
        assert(false);
        perror("sockAddrCalcHashWithPort");
        exit(1);
    }
    return result;
}

enum socket_address_type getHostAddrType(char *host)
{
    if (is_ipv4(host))
    {
        return kSatIPV4;
    }
    if (is_ipv6(host))
    {
        return kSatIPV6;
    }
    return kSatDomainName;
}

int parseIPWithSubnetMask(struct in6_addr *base_addr, const char *input, struct in6_addr *subnet_mask)
{
    char *slash;
    char *ip_part;
    char *subnet_part;
    char  input_copy[strlen(input) + 1];
    strcpy(input_copy, input);

    slash = strchr(input_copy, '/');
    if (slash == NULL)
    {
        fprintf(stderr, "Invalid input format.\n");
        return -1;
    }

    *slash      = '\0';
    ip_part     = input_copy;
    subnet_part = slash + 1;

    if (inet_pton(AF_INET, ip_part, base_addr) == 1)
    {
        // IPv4 address
        int prefix_length = atoi(subnet_part);
        if (prefix_length < 0 || prefix_length > 32)
        {
            fprintf(stderr, "Invalid subnet mask length.\n");
            return -1;
        }
        uint32_t mask = prefix_length > 0 ? htonl(~((1 << (32 - prefix_length)) - 1)) : 0;
        struct in_addr mask_addr = {.s_addr = mask};
        memcpy(subnet_mask, &mask_addr, 4);
        return 4;
    }

    if (inet_pton(AF_INET6, ip_part, base_addr) == 1)
    {
        // IPv6 address
        int prefix_length = atoi(subnet_part);
        if (prefix_length < 0 || prefix_length > 128)
        {
            fprintf(stderr, "Invalid subnet mask length.\n");
            return -1;
        }

        for (int i = 0; i < 16; i++)
        {
            int bits                     = prefix_length >= 8 ? 8 : prefix_length;
            ((uint8_t *) subnet_mask)[i] = bits == 0 ? 0 : (0xFF << (8 - bits));
            prefix_length -= bits;
        }

        return 6;
    }

    fprintf(stderr, "Invalid IP address.\n");
    return -1;
}

struct user_s *parseUserFromJsonObject(const cJSON *user_json)
{
    if (! cJSON_IsObject(user_json) || user_json->child == NULL)
    {
        return NULL;
    }
    user_t *user = globalMalloc(sizeof(user_t));
    memset(user, 0, sizeof(user_t));

    getStringFromJsonObjectOrDefault(&(user->name), user_json, "name", "EMPTY_NAME");
    getStringFromJsonObjectOrDefault(&(user->email), user_json, "email", "EMPTY_EMAIL");
    getStringFromJsonObjectOrDefault(&(user->notes), user_json, "notes", "EMTPY_NOTES");

    if (! getStringFromJsonObject(&(user->uid), user_json, "uid"))
    {
        globalFree(user);
        return NULL;
    }
    user->hash_uid = CALC_HASH_BYTES(user->uid, strlen(user->uid));

    bool enable;
    if (! getBoolFromJsonObject(&(enable), user_json, "enable"))
    {
        globalFree(user);
        return NULL;
    }
    user->enable = enable;
    // TODO (parse user) parse more fields from user like limits/dates/etc..
    return user;
}

bool verifyIpCdir(const char *ipc, struct logger_s *logger)
{
    unsigned int ipc_length = strlen(ipc);
    char        *slash      = strchr(ipc, '/');
    if (slash == NULL)
    {
        if (logger)
        {
            logger_print(logger, LOG_LEVEL_ERROR, "verifyIpCdir Error: Subnet prefix is missing in ip. \"%s\" + /xx",
                         ipc);
        }
        return false;
    }
    *slash = '\0';
    if (! is_ipaddr(ipc))
    {
        if (logger)
        {
            logger_print(logger, LOG_LEVEL_ERROR, "verifyIpCdir Error: \"%s\" is not a valid ip address", ipc);
        }
        return false;
    }

    bool is_v4 = is_ipv4(ipc);
    *slash     = '/';

    char *subnet_part   = slash + 1;
    int   prefix_length = atoi(subnet_part);

    if (is_v4 && (prefix_length < 0 || prefix_length > 32))
    {
        if (logger)
        {
            logger_print(
                logger, LOG_LEVEL_ERROR,
                "verifyIpCdir Error: Invalid subnet mask length for ipv4 %s prefix %d must be between 0 and 32", ipc,
                prefix_length);
        }
        return false;
    }
    if (! is_v4 && (prefix_length < 0 || prefix_length > 128))
    {
        if (logger)
        {
            logger_print(
                logger, LOG_LEVEL_ERROR,
                "verifyIpCdir Error: Invalid subnet mask length for ipv6 %s prefix %d must be between 0 and 128", ipc,
                prefix_length);
        }
        return false;
    }
    if (prefix_length > 0 && slash + 2 + (int) (log10(prefix_length)) < ipc + ipc_length)
    {
        if (logger)
        {
            logger_print(logger, LOG_LEVEL_WARN,
                         "verifyIpCdir Warning: the value \"%s\" looks incorrect, it has more data than ip/prefix",
                         ipc);
        }
    }
    return true;
}

dynamic_value_t parseDynamicStrValueFromJsonObject(const cJSON *json_obj, const char *key, size_t matchers, ...)
{

    dynamic_value_t result = {0};
    result.status          = kDvsEmpty;

    if (! cJSON_IsObject(json_obj) || json_obj->child == NULL)
    {
        return result;
    }

    const cJSON *jstr = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsString(jstr) && (jstr->valuestring != NULL))
    {

        va_list argp;
        va_start(argp, matchers);
        for (size_t mi = kDvsConstant + 1; mi < matchers + kDvsConstant + 1; mi++)
        {
            char *matcher = va_arg(argp, char *);
            if (strcmp(matcher, jstr->valuestring) == 0)
            {
                va_end(argp);
                result.status = mi;
                return result;
            }
        }

        va_end(argp);
        result.status    = kDvsConstant;
        result.value_ptr = globalMalloc(strlen(jstr->valuestring) + 1);
        strcpy(result.value_ptr, jstr->valuestring);
    }
    return result;
}

dynamic_value_t parseDynamicNumericValueFromJsonObject(const cJSON *json_obj, const char *key, size_t matchers, ...)
{

    dynamic_value_t result = {0};
    result.status          = kDvsEmpty;

    if (! cJSON_IsObject(json_obj) || json_obj->child == NULL)
    {
        return result;
    }
    const cJSON *jstr = cJSON_GetObjectItemCaseSensitive(json_obj, key);
    if (cJSON_IsString(jstr) && (jstr->valuestring != NULL))
    {

        va_list argp;
        va_start(argp, matchers);
        for (size_t mi = kDvsConstant + 1; mi < matchers + kDvsConstant + 1; mi++)
        {
            char *matcher = va_arg(argp, char *);
            if (strcmp(matcher, jstr->valuestring) == 0)
            {
                va_end(argp);
                result.status = mi;
                return result;
            }
        }

        va_end(argp);
        result.status = kDvsEmpty;
    }
    else if (cJSON_IsNumber(jstr))
    {
        result.status = kDvsConstant;
        result.value  = (size_t) jstr->valueint;
    }
    return result;
}

void destroyDynamicValue(const dynamic_value_t dy){
    if(dy.value_ptr){
        globalFree(dy.value_ptr);
    }
}

// blocking io
cmd_result_t execCmd(const char *str)
{
    FILE        *fp;
    cmd_result_t result = (cmd_result_t) {{0}, -1};
    char        *buf    = &(result.output[0]);
    /* Open the command for reading. */
#if defined(OS_UNIX)
    fp = popen(str, "r");
#else
    fp = _popen(str, "r");
#endif

    if (fp == NULL)
    {
        printf("Failed to run command \"%s\"\n", str);
        return (cmd_result_t) {{0}, -1};
    }

    int read = fscanf(fp, "%2047s", buf);
    (void) read;
#if defined(OS_UNIX)
    result.exit_code = pclose(fp);
#else
    result.exit_code = _pclose(fp);
#endif

    return result;
    /* close */
    // return 0 == pclose(fp);
}

bool checkCommandAvailable(const char *app)
{
    char b[300];
    sprintf(b, "command -v %s", app);
    cmd_result_t result = execCmd(b);
    return (result.exit_code == 0 && strlen(result.output) > 0);
}
