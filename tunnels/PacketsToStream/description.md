# PacketsToStream Node

`PacketsToStream` adapts a packet-oriented side of the chain to a stream-oriented side. It accepts discrete IPv4 packets from the previous side, forwards them through a data line toward the next node, and reconstructs packet boundaries again on the way back.

In practice, this tunnel is useful when one side of the chain works with packets while the next side expects a normal byte stream line.

## What It Does

- Accepts packet payload from the previous side.
- Creates and maintains one stream-facing line per worker toward the next node.
- Forwards each inbound packet as payload on that stream-facing line.
- Buffers return data from the next side until a complete IPv4 packet is available.
- Sends each reconstructed packet back to the previous side.
- Recreates the stream-facing line if that line is closed.

This tunnel is best thought of as a packet-to-data adapter with IPv4 packet reconstruction on the return path.

## Typical Placement

A common layout is:

- a packet-producing or packet-consuming node before `PacketsToStream`
- `PacketsToStream`
- stream-oriented transport or processing nodes after it

Typical use cases are chains where raw packet payload needs to move through nodes that operate on ordinary data lines instead of packet boundaries.

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

When traffic is first initialized on that worker:

- the tunnel creates a new line toward the next node
- stores it in the worker-local line state
- initializes the next node with that new line

That data line is then reused for packet payload arriving on the same worker.

### Data flow direction

- Packet side to data side: previous node -> `PacketsToStream` -> stream-facing line -> next node
- Data side back to packet side: next node -> `PacketsToStream` -> reconstructed packet -> previous node

From the packet side, this tunnel behaves like a packet-preserving adapter.

From the next side, it behaves like a normal line carrying payload data.

### Packet reconstruction

Return traffic from the next side is buffered in a read stream.

The tunnel then:

- waits until it has enough bytes for an IPv4 header
- reads the IPv4 total-length field
- waits until that many bytes are available
- emits one complete packet back to the previous side

This means packet boundary recovery is based on the IPv4 header's total packet length.

### Pause and resume behavior

When the downstream side pauses the stream-facing line, `PacketsToStream` marks the packet-facing side paused.

While paused:

- incoming packet payload is dropped instead of buffered

When resume arrives:

- packet forwarding continues normally

This keeps the tunnel simple and avoids unbounded packet buffering during backpressure.

### Finish and line recreation

If the stream-facing line toward the next node finishes:

- the old line is discarded
- the packet-side read buffer is cleared
- a fresh stream-facing line is created
- the next node is initialized again on that new line

This gives the tunnel a persistent packet-facing role while allowing the data-side line to be recreated transparently.

### Buffering limits

The read stream used for reconstructing IPv4 packets has a fixed overflow limit.

Current limit:

- `65536 * 2` bytes

If the buffered return data grows beyond that limit, the read stream is emptied.

## Notes And Caveats

- `PacketsToStream` has no tunnel-specific JSON settings today.
- Packet boundary reconstruction is based on IPv4 headers.
- This tunnel is most useful when the previous side is packet-oriented and the next side is stream-oriented.
- Upstream `est`, `pause`, `resume`, and `finish`, plus downstream `init`, are not part of the intended normal callback path for this tunnel.
