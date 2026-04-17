#include "structure.h"

#include "loggers/network_logger.h"

static void kcpPrintLog(const char *log, struct IKCPCB *kcp, void *user)
{
    discard user;

    LOGD("TcpOverUdpServer -> KCP[%d]: %s", kcp->conv, log);
}

void tcpoverudpserverLinestateInitialize(tcpoverudpserver_lstate_t *ls, line_t *l, tunnel_t *t)
{
    tcpoverudpserver_tstate_t *ts = tunnelGetState(t);

    ikcpcb *k_handle = ikcp_create(0, ls);
    if (k_handle == NULL)
    {
        LOGF("TcpOverUdpServer: failed to create KCP handle");
        terminateProgram(1);
    }

    /* configuring the high-efficiency KCP settings */

    ikcp_setoutput(k_handle, tcpoverudpserverKUdpOutput);

    ikcp_nodelay(k_handle, kTcpOverUdpServerKcpNodelay, kTcpOverUdpServerKcpInterval, kTcpOverUdpServerKcpResend,
                 kTcpOverUdpServerKcpFlowCtl);

    ikcp_wndsize(k_handle, kTcpOverUdpServerKcpSendWindow, kTcpOverUdpServerKcpRecvWindow);

    if (ikcp_setmtu(k_handle, tcpoverudpserverGetKcpMtu(ts)) != 0)
    {
        ikcp_release(k_handle);
        LOGF("TcpOverUdpServer: failed to set KCP MTU");
        terminateProgram(1);
    }

    k_handle->cwnd = kTcpOverUdpServerKcpSendWindow / 4;

    k_handle->writelog = kcpPrintLog;
    // k_handle->logmask = 0x0FFFFFFF; // Enable all logs

    k_handle->rx_minrto = 30;

    wtimer_t *k_timer = wtimerAdd(getWorkerLoop(lineGetWID(l)), tcpoverudpserverKcpLoopIntervalCallback,
                                  kTcpOverUdpServerKcpInterval, INFINITE);

    weventSetUserData(k_timer, ls);

    tcpoverudp_fec_encoder_t *fec_encoder = NULL;
    tcpoverudp_fec_decoder_t *fec_decoder = NULL;

    if (ts->fec_enabled)
    {
        fec_encoder = tcpoverudpFecEncoderCreate(ts->fec_data_shards, ts->fec_parity_shards);
        fec_decoder = tcpoverudpFecDecoderCreate(ts->fec_data_shards, ts->fec_parity_shards);

        if (fec_encoder == NULL || fec_decoder == NULL)
        {
            tcpoverudpFecEncoderDestroy(&fec_encoder);
            tcpoverudpFecDecoderDestroy(&fec_decoder);
            weventSetUserData(k_timer, NULL);
            wtimerDelete(k_timer);
            ikcp_release(k_handle);
            LOGF("TcpOverUdpServer: failed to initialize FEC state");
            terminateProgram(1);
        }
    }

    *ls = (tcpoverudpserver_lstate_t) {.k_handle     = k_handle,
                                       .k_timer      = k_timer,
                                       .fec_encoder  = fec_encoder,
                                       .fec_decoder  = fec_decoder,
                                       .tunnel       = t,
                                       .line         = l,
                                       .last_recv    = wloopNowMS(getWorkerLoop(lineGetWID(l))),
                                       .cq_d         = contextqueueCreate(),
                                       .cq_u         = contextqueueCreate(),
                                       .write_paused = false,
                                       .can_upstream = true,
                                       .ping_sent    = true};

    uint8_t ping_buf[kFrameHeaderLength] = {kFrameFlagPing};
    ikcp_send(ls->k_handle, (const char *) ping_buf, (int) sizeof(ping_buf));
}

void tcpoverudpserverLinestateDestroy(tcpoverudpserver_lstate_t *ls)
{
    if (ls->k_handle == NULL)
    {
        return;
    }

    if (ls->k_timer != NULL)
    {
        weventSetUserData(ls->k_timer, NULL);
        wtimerDelete(ls->k_timer);
    }

    contextqueueDestroy(&ls->cq_u);
    contextqueueDestroy(&ls->cq_d);

    tcpoverudpFecEncoderDestroy(&ls->fec_encoder);
    tcpoverudpFecDecoderDestroy(&ls->fec_decoder);

    if (ls->k_handle != NULL)
    {
        ikcp_release(ls->k_handle);
    }

    memoryZeroAligned32(ls, sizeof(tcpoverudpserver_lstate_t));
}
