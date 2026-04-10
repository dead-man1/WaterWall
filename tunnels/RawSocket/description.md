# RawSocket Node

`RawSocket` connects WaterWall to raw IPv4 packet capture and raw packet injection. It captures matching IP packets from the host networking stack and forwards them into the chain, and it can also inject raw IP packets coming from the chain back into the system.

This node is a layer-3 adapter rather than a connection-oriented tunnel.

## What It Does

- Creates a capture device for matching IPv4 packets.
- Creates a raw output device for sending raw IPv4 packets.
- Captures packets that match a configured IP filter.
- Forwards captured packets to the adjacent chain side.
- Writes raw IP packets from the chain out through the raw device.
- Applies checksum recalculation before writing when the line requests it.
- Optionally sets a firewall mark on the raw output device where supported.

## Typical Placement

`RawSocket` can be placed at either edge of a chain:

- if it is last in the chain, captured packets are forwarded to the previous node
- otherwise, captured packets are forwarded to the next node

Payload reaching `RawSocket` from upstream or downstream is treated as an IP packet and injected through the raw device.

## Configuration Example

```json
{
  "name": "raw-ip",
  "type": "RawSocket",
  "settings": {
    "capture-device-name": "capture-in",
    "raw-device-name": "raw-out",
    "capture-filter-mode": "source-ip",
    "capture-ip": "192.0.2.10",
    "mark": 10
  },
  "next": "next-node-name"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"RawSocket"`.

### `settings`

- `capture-filter-mode` `(string)`
  Filter mode for captured traffic.

  Parsed values are:
  - `"source-ip"`
  - `"dest-ip"`

  Important note: the current implementation only accepts `"source-ip"`. If `"dest-ip"` is selected, tunnel creation fails with a message telling you to use `TunDevice` for outgoing capture instead.

- `capture-ip` `(string)`
  IP address used by the capture device filter.

  In practice this should be treated as required. The implementation logs an error if it is missing, and the capture-device creation path expects a valid IP string.

## Optional `settings` Fields

- `capture-device-name` `(string)`
  User-visible or internal name for the capture device.

  Default: `"unnamed-capture-device"`

- `raw-device-name` `(string)`
  User-visible or internal name for the raw output device.

  Default: `"unnamed-raw-device"`

- `mark` `(integer)`
  Firewall mark used for the raw output device where the platform supports it.

  Default: `0`

## Detailed Behavior

### Capture path

During `onPrepare`, `RawSocket`:

- decides which adjacent tunnel should receive captured packets
- creates the capture device using `capture-ip`
- creates the raw output device
- brings both devices up

When a packet is captured:

- the packet is checked for IP version
- only IPv4 packets are currently forwarded by this path
- the packet is forwarded through the chosen adjacent side using the worker packet line

### Output path

When payload reaches `RawSocket` from upstream or downstream:

- the payload is treated as a raw IP packet
- checksum recalculation is performed if requested by the line
- the packet is written through the raw device

Both upstream and downstream payload handlers write to the same raw output device.

### Capture filter behavior

The capture device is configured using `capture-ip`.

Current implementation behavior:

- on Windows, the capture filter is built as `ip.SrcAddr == <capture-ip>`
- on Linux, netfilter rules are created around the configured `capture-ip`
- `capture-filter-mode` is parsed, but only the `source-ip` path is currently implemented

### Checksum behavior

Before writing a packet, `RawSocket` checks line flags:

- if `recalculate_checksum` is set, checksums are recomputed
- full packet checksum recalculation is attempted
- for fragmented IPv4 packets, transport checksum recalculation is skipped automatically and only the IPv4 header checksum is recomputed

### Why `TunDevice` may be a better fit for some cases

The current `RawSocket` implementation is focused on capturing inbound IPv4 traffic that matches a source-IP filter and injecting raw IPv4 packets.

If you need a virtual interface model or packet handling that is closer to routed interface traffic, `TunDevice` is usually the better match.

## Notes And Caveats

- The current receive path only forwards IPv4 packets.
- `capture-filter-mode` is effectively limited to `"source-ip"` right now.
- `capture-ip` should be provided explicitly.
- Platform support depends on the raw/capture backend available on the operating system.
