#pragma once

#include "hv/hplatform.h"
#include "hv/hsocket.h"
#include "hv/htime.h"
#include "hv/hatomic.h"
#include "hv/hmutex.h"

typedef uint64_t hash_t;

typedef struct ud_s
{
    _Atomic uint64_t u;
    _Atomic uint64_t d;
} ud_t;

typedef union
{
    ud_t max;
    _Atomic uint64_t max_total;
} traffic_limit_t;

typedef struct user_limit_s
{
    traffic_limit_t traffic;
    ud_t bandwidth;
    _Atomic uint64_t ip;
    _Atomic uint64_t devices;
    _Atomic uint64_t cons_in;
    _Atomic uint64_t cons_out;
} user_limit_t;

typedef struct user_time_info_s
{
    datetime_t create_date;
    datetime_t first_usage_date;
    datetime_t expire_date;
    _Atomic bool since_first_use;
} user_time_info_t;

typedef struct user_stat_s
{
    ud_t speed;
    ud_t traffic;
    _Atomic uint64_t ips;
    _Atomic uint64_t devices;
    _Atomic uint64_t cons_in;
    _Atomic uint64_t cons_out;

} user_stat_t;

typedef struct user_s
{
    struct cJSON *json;
    hmutex_t* fsync_lock;
    //-----------------
    char *name;
    char *email;
    char *notes;
    char *uid; // unique id
    int gid;   // group id
    hash_t hash_uid;

    _Atomic bool enable;
    user_limit_t limit;
    user_time_info_t timeinfo;
    user_stat_t stats;

} user_t;

enum socket_address_cmd
{
    SAC_CONNECT = 0X1,
    SAC_ASSOCIATE = 0X3,
};
enum socket_address_type
{
    SAT_IPV4 = 0X1,
    SAT_DOMAINNAME = 0X3,
    SAT_IPV6 = 0X4,

};


// all data we need to connect to somewhere
typedef struct socket_context_s
{
    uint8_t protocol; // IPPROTO_X
    enum socket_address_cmd acmd;
    enum socket_address_type atype;
    char *domain;
    bool resolved;
    sockaddr_u addr;
} socket_context_t;
