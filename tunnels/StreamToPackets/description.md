# StreamToPackets Node

`StreamToPackets` is the inverse adapter of `PacketsToStream`. It accepts a stream-oriented data line from the previous side, parses complete IPv4 packets out of that stream, and forwards those packets to the next side as packet payload.

On the return path, it sends packet payload back to the data side so the original line can continue carrying the packet stream.

## What It Does

- Accepts a normal data line from the previous side.
- Buffers incoming stream data until complete IPv4 packets can be extracted.
- Forwards each reconstructed packet to the next side.
- Tracks the active data line for each worker.
- Returns packet-side reply payload back to the previous data line.
- Drops or ignores packet output while the data side is paused.

This tunnel is useful when the previous side provides a stream that actually contains serialized IPv4 packets and the next side expects packet boundaries.

## Typical Placement

A common layout is:

- stream-oriented transport or processing nodes before `StreamToPackets`
- `StreamToPackets`
- packet-producing or packet-consuming nodes after it

Typical use cases are chains where IPv4 packets are embedded in a byte stream and need to be restored as discrete packets before continuing.

## Configuration Example

```json
{
  "name": "stream-to-packet",
  "type": "StreamToPackets",
  "settings": {},
  "next": "packet-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"StreamToPackets"`.

- `next` `(string)`
  The next node that should receive reconstructed packet payload.

### `settings`

There are no required tunnel-specific settings in the current implementation.

## Optional `settings` Fields

There are no tunnel-specific optional settings in the current implementation.

## Detailed Behavior

### Active data line per worker

When a data line is initialized from the previous side, `StreamToPackets` stores that line as the active line for the worker and creates a read stream for packet parsing.

As long as that line remains active, packet-side replies are sent back through it.

This lets the tunnel bridge one stream-oriented line on the left side to a packet-oriented side on the right.

### Data flow direction

- Data side to packet side: previous node -> `StreamToPackets` -> reconstructed IPv4 packets -> next node
- Packet side back to data side: next node -> `StreamToPackets` -> previous data line

From the previous side, this tunnel behaves like a normal stream line.

From the next side, it behaves like a packet-producing node.

### Packet extraction

Incoming upstream data is buffered until a complete IPv4 packet is available.

The tunnel then:

- checks for a complete IPv4 header
- reads the IPv4 total-length field
- waits until the entire packet is present
- forwards that packet as one payload unit to the next side

This means packet boundary detection is driven by the IPv4 header length field rather than by any external delimiter.

### Return path behavior

When packet payload arrives back from the next side:

- the tunnel forwards that payload to the active data line for the worker

This allows the original stream-facing side to continue carrying the packet byte stream in the opposite direction.

### Pause and resume behavior

When the upstream data line is paused:

- the worker-local packet return path is considered paused
- packet output back toward the data side is withheld until resume

When the upstream side resumes:

- packet forwarding to the data side continues normally

### Finish behavior

When the active upstream data line finishes:

- the worker-local active line reference is cleared
- the read stream used for packet parsing is destroyed

This resets the tunnel state for the next data line that may become active on that worker.

### Buffering limits

The packet parser uses a fixed-size read stream.

Current limit:

- `65536 * 2` bytes

If buffered data exceeds that size, the read stream is emptied.

## Notes And Caveats

- `StreamToPackets` has no tunnel-specific JSON settings today.
- Packet parsing is based on IPv4 headers.
- This tunnel is most useful when the previous side is stream-oriented and the next side expects packet boundaries.
- Upstream `est` plus downstream `init`, `fin`, `pause`, and `resume` are not part of the intended normal callback path for this tunnel.
