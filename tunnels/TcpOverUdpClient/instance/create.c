#include "structure.h"

#include "loggers/network_logger.h"

static bool tcpoverudpclientParseSettings(tcpoverudpclient_tstate_t *ts, node_t *node)
{
    const cJSON *settings = node->node_settings_json;

    *ts = (tcpoverudpclient_tstate_t) {.fec_enabled       = false,
                                       .fec_data_shards   = kTcpOverUdpClientFecDefaultDataShards,
                                       .fec_parity_shards = kTcpOverUdpClientFecDefaultParityShards};

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
        LOGF("JSON Error: TcpOverUdpClient->settings FEC requires 1..255 total shards with positive data/parity values");
        return false;
    }

    ts->fec_data_shards   = (uint8_t) data_shards;
    ts->fec_parity_shards = (uint8_t) parity_shards;

    if (tcpoverudpclientGetKcpMtu(ts) <= 0 || tcpoverudpclientGetKcpWriteMtu(ts) <= 0)
    {
        LOGF("TcpOverUdpClient: GLOBAL_MTU_SIZE is too small for KCP + FEC overhead");
        return false;
    }

    return true;
}

tunnel_t *tcpoverudpclientTunnelCreate(node_t *node)
{
    tunnel_t *t = tunnelCreate(node, sizeof(tcpoverudpclient_tstate_t), sizeof(tcpoverudpclient_lstate_t));

    t->fnInitU    = &tcpoverudpclientTunnelUpStreamInit;
    t->fnEstU     = &tcpoverudpclientTunnelUpStreamEst;
    t->fnFinU     = &tcpoverudpclientTunnelUpStreamFinish;
    t->fnPayloadU = &tcpoverudpclientTunnelUpStreamPayload;
    t->fnPauseU   = &tcpoverudpclientTunnelUpStreamPause;
    t->fnResumeU  = &tcpoverudpclientTunnelUpStreamResume;

    t->fnInitD    = &tcpoverudpclientTunnelDownStreamInit;
    t->fnEstD     = &tcpoverudpclientTunnelDownStreamEst;
    t->fnFinD     = &tcpoverudpclientTunnelDownStreamFinish;
    t->fnPayloadD = &tcpoverudpclientTunnelDownStreamPayload;
    t->fnPauseD   = &tcpoverudpclientTunnelDownStreamPause;
    t->fnResumeD  = &tcpoverudpclientTunnelDownStreamResume;

    t->onPrepare = &tcpoverudpclientTunnelOnPrepair;
    t->onStart   = &tcpoverudpclientTunnelOnStart;
    t->onDestroy = &tcpoverudpclientTunnelDestroy;

    
    tcpoverudpclient_tstate_t *ts = tunnelGetState(t);

    if (! tcpoverudpclientParseSettings(ts, node))
    {
        tunnelDestroy(t);
        return NULL;
    }

    ikcp_allocator(&memoryAllocate,
                   &memoryFree);

    return t;
}
