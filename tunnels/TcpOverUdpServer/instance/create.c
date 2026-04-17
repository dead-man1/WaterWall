#include "structure.h"

#include "loggers/network_logger.h"

static bool tcpoverudpserverParseSettings(tcpoverudpserver_tstate_t *ts, node_t *node)
{
    const cJSON *settings = node->node_settings_json;

    *ts = (tcpoverudpserver_tstate_t) {.fec_enabled       = false,
                                       .fec_data_shards   = kTcpOverUdpServerFecDefaultDataShards,
                                       .fec_parity_shards = kTcpOverUdpServerFecDefaultParityShards};

    if (! cJSON_IsObject(settings))
    {
        return true;
    }

    getBoolFromJsonObjectOrDefault(&ts->fec_enabled, settings, "fec", false);

    int data_shards = ts->fec_data_shards;
    int parity_shards = ts->fec_parity_shards;
    getIntFromJsonObjectOrDefault(&data_shards, settings, "fec-data-shards", data_shards);
    getIntFromJsonObjectOrDefault(&parity_shards, settings, "fec-parity-shards", parity_shards);

    if (! ts->fec_enabled)
    {
        return true;
    }

    if (data_shards <= 0 || parity_shards <= 0 || data_shards + parity_shards > 255)
    {
        LOGF("JSON Error: TcpOverUdpServer->settings FEC requires 1..255 total shards with positive data/parity values");
        return false;
    }

    ts->fec_data_shards   = (uint8_t) data_shards;
    ts->fec_parity_shards = (uint8_t) parity_shards;

    if (tcpoverudpserverGetKcpMtu(ts) <= 0 || tcpoverudpserverGetKcpWriteMtu(ts) <= 0)
    {
        LOGF("TcpOverUdpServer: GLOBAL_MTU_SIZE is too small for KCP + FEC overhead");
        return false;
    }

    return true;
}

tunnel_t *tcpoverudpserverTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(tcpoverudpserver_tstate_t), sizeof(tcpoverudpserver_lstate_t));

    t->fnInitU    = &tcpoverudpserverTunnelUpStreamInit;
    t->fnEstU     = &tcpoverudpserverTunnelUpStreamEst;
    t->fnFinU     = &tcpoverudpserverTunnelUpStreamFinish;
    t->fnPayloadU = &tcpoverudpserverTunnelUpStreamPayload;
    t->fnPauseU   = &tcpoverudpserverTunnelUpStreamPause;
    t->fnResumeU  = &tcpoverudpserverTunnelUpStreamResume;

    t->fnInitD    = &tcpoverudpserverTunnelDownStreamInit;
    t->fnEstD     = &tcpoverudpserverTunnelDownStreamEst;
    t->fnFinD     = &tcpoverudpserverTunnelDownStreamFinish;
    t->fnPayloadD = &tcpoverudpserverTunnelDownStreamPayload;
    t->fnPauseD   = &tcpoverudpserverTunnelDownStreamPause;
    t->fnResumeD  = &tcpoverudpserverTunnelDownStreamResume;

    t->onPrepare = &tcpoverudpserverTunnelOnPrepair;
    t->onStart   = &tcpoverudpserverTunnelOnStart;
    t->onDestroy = &tcpoverudpserverTunnelDestroy;

    tcpoverudpserver_tstate_t *ts = tunnelGetState(t);

    if (! tcpoverudpserverParseSettings(ts, node))
    {
        tunnelDestroy(t);
        return NULL;
    }

    ikcp_allocator(&memoryAllocate, &memoryFree);

    return t;
}
