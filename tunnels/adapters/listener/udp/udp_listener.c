#include "udp_listener.h"
#include "buffer_pool.h"
#include "idle_table.h"
#include "loggers/network_logger.h"
#include "managers/socket_manager.h"
#include "tunnel.h"
#include "utils/jsonutils.h"
#include "utils/sockutils.h"

// enable profile to see some time info
// #define PROFILE 1

typedef struct udp_listener_state_s
{
    // settings
    char    *address;
    int      multiport_backend;
    uint16_t port_min;
    uint16_t port_max;
    char   **white_list_raddr;
    char   **black_list_raddr;

} udp_listener_state_t;

typedef struct udp_listener_con_state_s
{
    hloop_t       *loop;
    tunnel_t      *tunnel;
    hio_t         *io;
    idle_table_t  *table;
    line_t        *line;
    idle_item_t   *idle_handle;
    buffer_pool_t *buffer_pool;
    bool           established;
    bool           first_packet_sent;
} udp_listener_con_state_t;

static void cleanup(udp_listener_con_state_t *cstate)
{

    if (cstate->idle_handle)
    {
        removeIdleItemByHandle(cstate->table, cstate->idle_handle);
    }
    free(cstate);
}

static void upStream(tunnel_t *self, context_t *c)
{
    if (c->payload != NULL)
    {
#ifdef PROFILE
        if (c->first)
        {
            struct timeval tv1, tv2;
            gettimeofday(&tv1, NULL);
            {
                self->up->upStream(self->up, c);
            }
            gettimeofday(&tv2, NULL);
            double time_spent = (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 + (double) (tv2.tv_sec - tv1.tv_sec);
            LOGD("UdpListener: upstream took %d ms", (int) (time_spent * 1000));
            return;
        }
#endif
    }
    else
    {
        if (c->fin)
        {

            udp_listener_con_state_t *cstate = CSTATE(c);
            cleanup(cstate);
            CSTATE_MUT(c) = NULL;
            destroyLine(c->line);
        }
    }

    self->up->upStream(self->up, c);
}

static void downStream(tunnel_t *self, context_t *c)
{
    udp_listener_con_state_t *cstate = CSTATE(c);

    if (c->payload != NULL)
    {
        postUdpWrite(cstate->io, c->payload);
        destroyContext(c);
    }
    else
    {

        if (c->est)
        {
            cstate->established = true;
            destroyContext(c);
            return;
        }
        if (c->fin)
        {
            hio_t *io     = cstate->io;
            CSTATE_MUT(c) = NULL;
            cleanup(cstate);
            destroyLine(c->line);
            destroyContext(c);
            return;
        }
    }
}

static void onUdpConnectonExpire(idle_item_t *idle_udp)
{
    
    udp_listener_con_state_t *cstate = idle_udp->userdata;
    assert(cstate != NULL);
    LOGD("UdpListener: expired idle udp FD:%x ", (int) hio_fd(cstate->io));
    cstate->idle_handle = NULL; // its freed by the table after return
    tunnel_t  *self     = (cstate)->tunnel;
    line_t    *line     = (cstate)->line;
    context_t *context  = newFinContext(line);
    self->upStream(self, context);
}

static udp_listener_con_state_t *newConnection(uint8_t tid, tunnel_t *self, hio_t *io, idle_table_t *table,
                                               uint8_t real_localport)
{

    line_t                   *line        = newLine(tid);
    udp_listener_con_state_t *cstate      = malloc(sizeof(udp_listener_con_state_t));
    cstate->loop                          = loops[tid];
    cstate->line                          = line;
    cstate->buffer_pool                   = getThreadBufferPool(tid);
    cstate->io                            = io;
    cstate->table                         = table;
    cstate->tunnel                        = self;
    cstate->established                   = false;
    cstate->first_packet_sent             = false;
    line->chains_state[self->chain_index] = cstate;
    line->src_ctx.address_type            = line->src_ctx.address.sa.sa_family == AF_INET ? kSatIPV4 : kSatIPV6;
    line->src_ctx.address_protocol        = kSapUdp;
    line->src_ctx.address                 = *(sockaddr_u *) hio_peeraddr(io);
    sockaddr_set_port(&(line->src_ctx.address), real_localport);

    struct sockaddr log_localaddr = *hio_localaddr(cstate->io);
    sockaddr_set_port((sockaddr_u *) &(log_localaddr), real_localport);

    char localaddrstr[SOCKADDR_STRLEN] = {0};
    char peeraddrstr[SOCKADDR_STRLEN]  = {0};

    LOGD("UdpListener: Accepted FD:%x  [%s] <= [%s]", (int) hio_fd(cstate->io),
         SOCKADDR_STR(&log_localaddr, localaddrstr), SOCKADDR_STR(hio_peeraddr(io), peeraddrstr));

    // send the init packet
    lockLine(line);
    {
        context_t *context = newInitContext(line);
        self->upStream(self, context);
        if (! isAlive(line))
        {
            LOGW("UdpListener: socket just got closed by upstream before anything happend");
            unLockLine(line);
            return NULL;
        }
    }
    unLockLine(line);
    return cstate;
}

static void onFilteredRecv(hevent_t *ev)
{
    udp_payload_t *data          = (udp_payload_t *) hevent_userdata(ev);
    hash_t         peeraddr_hash = sockAddrCalcHash((sockaddr_u *) hio_peeraddr(data->sock->io));

    idle_item_t *idle = getIdleItemByHash(data->sock->udp_table, peeraddr_hash);
    if (idle == NULL)
    {
        udp_listener_con_state_t *con =
            newConnection(data->tid, data->tunnel, data->sock->io, data->sock->udp_table, data->real_localport);
        if (! con)
        {
            reuseBuffer(getThreadBufferPool(data->tid), data->buf);
            free(data);
            return;
        }
        con->idle_handle = newIdleItem(data->sock->udp_table, peeraddr_hash, con, onUdpConnectonExpire, data->tid,
                                       (uint64_t) 70 * 1000);
    }
    tunnel_t                 *self    = data->tunnel;
    udp_listener_con_state_t *con     = idle->userdata;
    context_t                *context = newContext(con->line);
    context->payload                  = data->buf;
    self->upStream(self, context);
    free(data);
}

static void parsePortSection(udp_listener_state_t *state, const cJSON *settings)
{
    const cJSON *port_json = cJSON_GetObjectItemCaseSensitive(settings, "port");
    if ((cJSON_IsNumber(port_json) && (port_json->valuedouble != 0)))
    {
        // single port given as a number
        state->port_min = (int) port_json->valuedouble;
        state->port_max = (int) port_json->valuedouble;
    }
    else
    {
        if (cJSON_IsArray(port_json) && cJSON_GetArraySize(port_json) == 2)
        {
            // multi port given
            const cJSON *port_minmax;
            int          i = 0;
            cJSON_ArrayForEach(port_minmax, port_json)
            {
                if (! (cJSON_IsNumber(port_minmax) && (port_minmax->valuedouble != 0)))
                {
                    LOGF("JSON Error: UdpListener->settings->port (number-or-array field) : The data was empty or "
                         "invalid");
                    exit(1);
                }
                if (i == 0)
                {
                    state->port_min = (int) port_minmax->valuedouble;
                }
                else if (i == 1)
                {
                    state->port_max = (int) port_minmax->valuedouble;
                }

                i++;
            }
        }
        else
        {
            LOGF("JSON Error: UdpListener->settings->port (number-or-array field) : The data was empty or invalid");
            exit(1);
        }
    }
}
tunnel_t *newUdpListener(node_instance_context_t *instance_info)
{
    udp_listener_state_t *state = malloc(sizeof(udp_listener_state_t));
    memset(state, 0, sizeof(udp_listener_state_t));
    const cJSON *settings = instance_info->node_settings_json;

    if (! (cJSON_IsObject(settings) && settings->child != NULL))
    {
        LOGF("JSON Error: UdpListener->settings (object field) : The object was empty or invalid");
        return NULL;
    }

    if (! getStringFromJsonObject(&(state->address), settings, "address"))
    {
        LOGF("JSON Error: UdpListener->settings->address (string field) : The data was empty or invalid");
        return NULL;
    }
    socket_filter_option_t filter_opt = {0};

    filter_opt.multiport_backend = kMultiportBackendNothing;
    parsePortSection(state, settings);
    if (state->port_max != 0)
    {
        filter_opt.multiport_backend = kMultiportBackendDefault;
        dynamic_value_t dy_mb =
            parseDynamicStrValueFromJsonObject(settings, "multiport-backend", 2, "iptables", "socket");
        if (dy_mb.status == 2)
        {
            filter_opt.multiport_backend = kMultiportBackendIptables;
        }
        if (dy_mb.status == 3)
        {
            filter_opt.multiport_backend = kMultiportBackendSockets;
        }
    }

    filter_opt.white_list_raddr = NULL;
    const cJSON *wlist          = cJSON_GetObjectItemCaseSensitive(settings, "whitelist");
    if (cJSON_IsArray(wlist))
    {
        size_t len = cJSON_GetArraySize(wlist);
        if (len > 0)
        {
            char **list = (char **) malloc(sizeof(char *) * (len + 1));
            memset((void *) list, 0, sizeof(char *) * (len + 1));
            list[len]              = 0x0;
            int          i         = 0;
            const cJSON *list_item = NULL;
            cJSON_ArrayForEach(list_item, wlist)
            {
                unsigned int list_item_len = 0;
                if (! getStringFromJson(&(list[i]), list_item) || ! verifyIpCdir(list[i], getNetworkLogger()))
                {
                    LOGF("JSON Error: UdpListener->settings->whitelist (array of strings field) index %d : The data "
                         "was empty or invalid",
                         i);
                    exit(1);
                }

                i++;
            }

            filter_opt.white_list_raddr = list;
        }
    }

    filter_opt.host             = state->address;
    filter_opt.port_min         = state->port_min;
    filter_opt.port_max         = state->port_max;
    filter_opt.proto            = kSapUdp;
    filter_opt.black_list_raddr = NULL;

    tunnel_t *t   = newTunnel();
    t->state      = state;
    t->upStream   = &upStream;
    t->downStream = &downStream;
    registerSocketAcceptor(t, filter_opt, onFilteredRecv);

    atomic_thread_fence(memory_order_release);
    return t;
}

api_result_t apiUdpListener(tunnel_t *self, const char *msg)
{
    (void) (self);
    (void) (msg);
    return (api_result_t){0};
}

tunnel_t *destroyUdpListener(tunnel_t *self)
{
    (void) (self);
    return NULL;
}
tunnel_metadata_t getMetadataUdpListener()
{
    return (tunnel_metadata_t){.version = 0001, .flags = kNodeFlagChainHead};
}