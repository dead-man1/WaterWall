#pragma once

/*
 * Declares packet-oriented tunnel defaults used by L3/L4 style nodes.
 */

#include "tunnel.h"

/**
 * @brief Create a packet tunnel with packet-specific default routines.
 *
 * @param node Owner node.
 * @param tstate_size Tunnel-local state size.
 * @param lstate_size Must be zero for packet tunnels.
 * @return tunnel_t* Created packet tunnel.
 */
tunnel_t *packettunnelCreate(node_t *node, uint16_t tstate_size, uint16_t lstate_size);

/**
 * @brief Forward upstream init to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultUpStreamInit(tunnel_t *self, line_t *line);

/**
 * @brief Forward upstream establish to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultUpStreamEst(tunnel_t *self, line_t *line);

/**
 * @brief Forward upstream finish to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultUpStreamFin(tunnel_t *self, line_t *line);

/**
 * @brief Default upstream payload routine that must be overridden by packet nodes.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 * @param payload Packet payload.
 */
void packettunnelDefaultUpStreamPayload(tunnel_t *self, line_t *line, sbuf_t *payload);

/**
 * @brief Forward upstream pause to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultUpStreamPause(tunnel_t *self, line_t *line);

/**
 * @brief Forward upstream resume to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultUpStreamResume(tunnel_t *self, line_t *line);

/**
 * @brief Default downstream init routine that is invalid for packet tunnels.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultDownStreamInit(tunnel_t *self, line_t *line);

/**
 * @brief Forward downstream establish to the previous tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultDownStreamEst(tunnel_t *self, line_t *line);

/**
 * @brief Handle downstream finish for packet tunnels.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultDownStreamFin(tunnel_t *self, line_t *line);

/**
 * @brief Default downstream payload routine that must be overridden by packet nodes.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 * @param payload Packet payload.
 */
void packettunnelDefaultDownStreamPayload(tunnel_t *self, line_t *line, sbuf_t *payload);

/**
 * @brief Forward downstream pause to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultDownStreamPause(tunnel_t *self, line_t *line);

/**
 * @brief Forward downstream resume to the next tunnel.
 *
 * @param self Packet tunnel.
 * @param line Connection line.
 */
void packettunnelDefaultDownStreamResume(tunnel_t *self, line_t *line);
