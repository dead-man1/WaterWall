# PacketsToStream Node

`PacketsToStream` adapts a packet-oriented side of the chain to a stream-oriented side.

The current implementation frames each packet by adding a **2-byte length prefix** (network byte order / big-endian), then sends that framed data over a normal stream line.

On the reverse path, it reads the same 2-byte length prefix and reconstructs packet boundaries.

## What It Does

- Accepts packet payload from the previous side.
- Creates and maintains one stream-facing line per worker toward the next node.
- For each outgoing packet, prepends a 2-byte packet size field.
- Buffers return stream data and extracts complete framed packets.
- Sends each reconstructed packet back to the previous side.
- Recreates the stream-facing line if that line is closed.

This tunnel is a packet-to-stream adapter based on explicit length framing, not IPv4 header parsing.

## Typical Placement

A common layout is:

- a packet-producing or packet-consuming node before `PacketsToStream`
- `PacketsToStream`
- stream-oriented transport or processing nodes after it

### Basic two-server use case

- `server1`: `TunDevice` -> `PacketsToStream` -> `TcpConnector`
- `server2`: `TcpListener` -> `StreamToPackets` -> `TunDevice`

This works because `PacketsToStream` and `StreamToPackets` use the same 2-byte framing format.

## Configuration Example

```json
{
  "name": "packet-to-stream",
  "type": "PacketsToStream",
  "settings": {},
  "next": "stream-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"PacketsToStream"`.

- `next` `(string)`
  The next node that should receive the forwarded data payload.

### `settings`

There are no required tunnel-specific settings in the current implementation.

## Optional `settings` Fields

There are no tunnel-specific optional settings in the current implementation.

## Detailed Behavior

### Worker-local data line

`PacketsToStream` maintains a stream-facing line for each worker as needed.

When traffic is initialized on that worker:

- the tunnel creates a new line toward the next node
- stores it in worker-local state
- initializes the next node with that new line

That data line is then reused for packet payload arriving on the same worker.

### Data flow direction

- Packet side to data side: previous node -> `PacketsToStream` -> stream-facing line -> next node
- Data side back to packet side: next node -> `PacketsToStream` -> reconstructed packet -> previous node

From the packet side, this tunnel behaves like a packet-preserving adapter.

From the next side, it behaves like a normal line carrying framed bytes.

### Framing format

Each packet is encoded as:

- 2 bytes: packet length (`uint16`, network byte order)
- N bytes: raw packet payload

Maximum packet size accepted by this node is limited by `kMaxAllowedPacketLength`.

### Checksum recalc behavior

Before sending packet payload to the stream side:

- if `line->recalculate_checksum` is enabled and payload is IPv4,
- full packet checksum is recalculated,
- then normal framing is applied.

### Frame decoding on return path

Return traffic from the next side is buffered in a read stream.

The tunnel then:

- waits for at least 2 bytes
- reads framed packet length
- waits until the whole frame (`2 + length`) is available
- emits one packet back to the previous side

Packet boundary recovery is based on this 2-byte prefix.

### Pause and resume behavior

When the downstream side pauses the stream-facing line, `PacketsToStream` marks the packet-facing side paused.

While paused:

- incoming packet payload is dropped instead of buffered

When resume arrives:

- packet forwarding continues normally

### Finish and line recreation

If the stream-facing line toward the next node finishes:

- the old line is discarded
- the read buffer is cleared
- a fresh stream-facing line is created
- the next node is initialized again on that new line

This keeps the packet-facing role stable while allowing automatic recreation of the stream-side line.

### Buffering limits

The read stream uses a fixed overflow limit.

Current limit:

- `65536 * 2` bytes

If buffered return data grows beyond that limit, the read stream is emptied.

## Notes And Caveats

- `PacketsToStream` has no tunnel-specific JSON settings today.
- This node is framing-based (`2-byte length + payload`), not IPv4-length based.
- Pair it with `StreamToPackets` on the other side to restore packet boundaries.
- Upstream `est`, `pause`, `resume`, and `finish`, plus downstream `init`, are not part of the intended normal callback path for this tunnel.
