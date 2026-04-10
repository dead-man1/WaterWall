# TcpOverUdpServer Node

`TcpOverUdpServer` is the server-side peer of `TcpOverUdpClient`. It receives UDP-style packet traffic carrying KCP data, reconstructs the original byte stream, and forwards that stream to the next node.

In practice, this node is used together with `TcpOverUdpClient` on the other side of the packet path.

## What It Does

- Accepts packet payload from the previous node.
- Feeds those packets into KCP.
- Reconstructs a TCP-like stream from the KCP session.
- Forwards the reconstructed stream to the next node.
- Accepts downstream stream replies, re-encodes them into KCP, and sends them back as packets.
- Uses internal ping and close frames to maintain the session.

This node expects the previous side of the chain to provide packet-preserving transport.

## Typical Placement

A common layout is:

- a UDP-capable tunnel or transport before `TcpOverUdpServer`
- `TcpOverUdpServer`
- one or more stream-facing service nodes after it

It should usually sit opposite `TcpOverUdpClient`.

## Configuration Example

```json
{
  "name": "tcp-over-udp-server",
  "type": "TcpOverUdpServer",
  "settings": {},
  "next": "service-stream-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"TcpOverUdpServer"`.

- `next` `(string)`
  The next node that should receive the reconstructed stream.

### `settings`

There are no required tunnel-specific settings in the current implementation.

## Optional `settings` Fields

There are no tunnel-specific optional settings in the current implementation.

## Detailed Behavior

### KCP transport model

When a line is initialized, `TcpOverUdpServer` creates a KCP session, starts the KCP timer loop, and queues an initial ping frame.

Incoming packet payloads from the previous node are fed into KCP. Completed KCP payloads are then read out and interpreted using a 1-byte internal frame flag.

Replies from the next node are chunked, wrapped, and sent back through KCP toward the previous side.

### Internal frame flags

Inside the KCP payload, this tunnel uses:

- `0x00`: data
- `0xF0`: ping
- `0xFF`: close

Data frames carry stream bytes after the flag byte.

### Current hard-coded KCP settings

The current implementation uses built-in KCP settings and does not expose them in JSON:

- `nodelay = 1`
- `interval = 10 ms`
- `resend = 2`
- `flowctl = 0`
- send window `2048`
- receive window `2048`
- ping interval `3000 ms`
- no-receive timeout `6000 ms`

KCP MTU is also taken from `GLOBAL_MTU_SIZE`.

### Data flow direction

- Packet side to stream side: previous node -> `TcpOverUdpServer` -> next node
- Stream replies back to packet side: next node -> `TcpOverUdpServer` -> previous node

Incoming KCP data frames are decoded and forwarded to the next node as stream payload.

Outgoing stream payload is split into KCP-friendly chunks, tagged as data frames, and sent back through the previous side.

### Close and timeout behavior

If a close frame is received from the remote peer, `TcpOverUdpServer` destroys line state and finishes both directions.

If the service-facing side finishes, `TcpOverUdpServer` sends a close frame over KCP, flushes KCP output, destroys line state, and finishes the previous side.

Idle handling is the same basic model as the client side:

- after `3000 ms` of no receive activity, a ping is sent if needed
- after `6000 ms` of no receive activity, the line is closed

### Backpressure behavior

If the KCP send queue grows too large, `TcpOverUdpServer` schedules a pause toward the next side. When queued KCP data drops back below the internal threshold, it schedules resume.

## Notes And Caveats

- `TcpOverUdpServer` is intended to be paired with `TcpOverUdpClient`.
- There are no tunnel-specific JSON settings today.
- The previous node should preserve packet boundaries.
- `UpStreamEst` and `DownStreamInit` are disabled in the current implementation.
