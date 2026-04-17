# TcpOverUdpClient Node

`TcpOverUdpClient` carries a TCP-like byte stream over a UDP path by running KCP internally. It accepts stream data from the previous node, feeds it into KCP, and sends the resulting packets through the next node.

In practice, this node is used together with `TcpOverUdpServer` on the remote side.

## What It Does

- Accepts stream payload from the previous node.
- Encodes that stream into KCP segments.
- Adds a 1-byte internal frame flag to KCP payloads.
- Sends the resulting datagrams through the next node.
- Receives datagrams back from the next node, feeds them into KCP, and reconstructs the stream.
- Uses periodic ping frames and an idle timeout to detect dead peers.

This node expects the next side of the chain to preserve packet boundaries. In practice that means a UDP-capable transport path.

## Typical Placement

A common layout is:

- a stream-producing node before `TcpOverUdpClient`
- `TcpOverUdpClient`
- a UDP-capable tunnel or transport after it
- `TcpOverUdpServer` on the remote side
- service-facing stream nodes after `TcpOverUdpServer`

This pair is useful when you want stream semantics on top of a datagram path.

## Configuration Example

```json
{
  "name": "tcp-over-udp-client",
  "type": "TcpOverUdpClient",
  "settings": {
    "fec": true,
    "fec-data-shards": 10,
    "fec-parity-shards": 3
  },
  "next": "udp-path-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"TcpOverUdpClient"`.

- `next` `(string)`
  The next node that should carry the UDP-style packet traffic.

### `settings`

There are no required tunnel-specific settings.

## Optional `settings` Fields

- `fec` `(boolean)`
  Enables Reed-Solomon forward error correction around the KCP datagrams.
  If omitted or `false`, the tunnel keeps the old KCP-only behavior.

- `fec-data-shards` `(number)`
  Number of source shards per FEC block when `fec` is enabled.
  Default: `10`

- `fec-parity-shards` `(number)`
  Number of parity shards per FEC block when `fec` is enabled.
  Default: `3`

## Detailed Behavior

### KCP transport model

When a line is initialized, `TcpOverUdpClient` creates a KCP session, starts a periodic timer, and immediately queues an internal ping frame.

Stream payload arriving from the previous node is split into chunks small enough to fit into the configured KCP write MTU and then sent through KCP.

The remote `TcpOverUdpServer` performs the reverse operation and exposes the reconstructed stream again.

### Internal frame flags

Inside the KCP payload, this tunnel uses a 1-byte flag:

- `0x00`: data
- `0xF0`: ping
- `0xFF`: close

Data frames carry real stream bytes after the first byte.

Ping frames are only used as keepalive markers. Close frames request shutdown of the paired line.

### Current hard-coded KCP settings

The current implementation does not expose KCP tuning in JSON. It uses these built-in values:

- `nodelay = 1`
- `interval = 10 ms`
- `resend = 2`
- `flowctl = 0`
- send window `2048`
- receive window `2048`
- ping interval `3000 ms`
- no-receive timeout `6000 ms`

The code also sets KCP MTU from `GLOBAL_MTU_SIZE` and uses an effective write payload size of roughly:

- `GLOBAL_MTU_SIZE - 20 - 8 - 24 - 1`

If FEC is enabled, the tunnel subtracts the FEC wire overhead from the outer KCP/UDP packet budget so the transport stays within the same path MTU envelope.

### Optional FEC layer

When `fec` is enabled, the tunnel wraps each outbound KCP datagram in a Reed-Solomon FEC packet and emits parity packets after each configured data-shard block.

On receive, it:

- accepts FEC-wrapped KCP datagrams
- feeds normal data packets into KCP immediately
- tries to recover missing KCP datagrams from parity packets
- drops invalid FEC packets conservatively

When `fec` is disabled, none of this extra framing is used and the node behaves exactly like the old KCP-only implementation.

### Data flow direction

- Stream to UDP/KCP side: previous node -> `TcpOverUdpClient` -> next node
- UDP/KCP side back to stream: next node -> `TcpOverUdpClient` -> previous node

On the send side, `TcpOverUdpClient` breaks the stream into KCP-friendly chunks and forwards the resulting packet traffic to the next node.

On the receive side, it feeds incoming packet payloads into KCP, drains completed KCP data, strips the 1-byte flag, and forwards reconstructed stream payload downstream to the previous node.

### Close and timeout behavior

If the previous side finishes, `TcpOverUdpClient` sends an internal close frame through KCP, flushes pending KCP output, destroys its line state, and finishes the next side.

If a close frame is received from the remote side, it destroys line state and finishes both directions.

If no data is received for too long:

- after `3000 ms`, a ping is sent if one was not already sent
- after `6000 ms` without receive activity, the line is closed

### Backpressure behavior

If the KCP send queue grows beyond the current internal limit, `TcpOverUdpClient` schedules a pause toward the previous side. Once the KCP queue drains enough, it schedules a resume.

This is how the tunnel prevents unbounded growth while the packet path is congested.

## Notes And Caveats

- `TcpOverUdpClient` is intended to be paired with `TcpOverUdpServer`.
- FEC must be enabled on both peers with matching shard settings.
- The next node should preserve datagram boundaries.
- `UpStreamEst` and `DownStreamInit` are disabled in the current implementation.
