# PingClient Node

`PingClient` is a layer-3 packet tunnel. On the upstream path it transforms IPv4 packets according to a configured ICMP-related strategy, and on the downstream path it applies the inverse logic for the matching peer traffic.

It is a pure packet tunnel created with `packettunnelCreate()`, so it does not create per-connection line state and it works on the worker packet lines supplied by the chain.

## What It Does

- upstream uses one of four JSON-controlled strategies
- downstream reverses that strategy for matching packets
- only IPv4 is supported at runtime
- any IPv6 packet that reaches `PingClient` is logged and dropped
- `xor-byte` still applies only to the ICMP payload modes
- `roundup-size` still applies only to the ICMP payload modes
- `identifier`, `sequence-start`, and `ipv4-id-start` are only meaningful for the ICMP envelope modes

## `strategy`

### `wrap-in-new-ip-and-icmp-header`

- wraps the whole inner IPv4 packet as:
  `new outer IPv4 header -> ICMP echo header -> original IPv4 packet`
- requires both `source` and `dest` in `settings`
- uses those configured IPv4 addresses for the outer packet
- on decapsulation, the packet must also match the configured outer source and destination
- if ICMP framing and identifier match but source or destination does not, `PingClient` logs a runtime warning and leaves the packet unchanged for the previous node
- source/destination verification accepts both the configured direction and the reversed direction

### `wrap-in-icmp-header-and-reuse-ipv4-addresses`

- still emits a full outer IPv4 packet plus ICMP header
- does not ask for `source` or `dest`
- keeps outer source and destination from the current IPv4 packet
- keeps the configured `ttl` and `tos` behavior of the existing tunnel
- still validates that the recovered payload is a valid inner IPv4 packet before decapsulation succeeds

Important note:
This mode cannot literally reuse the same IPv4 header in place. To restore the original packet losslessly on the peer, the original IPv4 header must remain inside the ICMP payload, so `PingClient` still creates a fresh 20-byte outer IPv4 header.

### `wrap-in-only-icmp-header`

- emits the same outer IPv4 plus ICMP envelope shape as the normal ICMP modes
- does not ask for `source` or `dest`
- keeps outer source and destination from the current IPv4 packet
- keeps configured `ttl` and `tos` for the new outer IPv4 header
- treats the recovered ICMP payload as opaque bytes on decapsulation instead of insisting that it is a valid IPv4 packet

### `change-only-ipv4-protocol-number`

- does not add an ICMP header and does not prepend a new IPv4 header
- only swaps the IPv4 protocol number in place
- requires `swap-protocol`
- upstream changes packets whose current IPv4 protocol matches `swap-protocol` into `ICMP`
- downstream changes matching `ICMP` packets back to `swap-protocol`
- this mode does not use `identifier`, `sequence-start`, `ipv4-id-start`, `xor-byte`, or `roundup-size`

`swap-protocol` accepts:

- `"TCP"`
- `"UDP"`
- `"ICMP"`
- an integer protocol number between `0` and `255`

## Optional `settings`

- `strategy` `(string)`
  Controls packet transformation mode.
  Default: `wrap-in-icmp-header-and-reuse-ipv4-addresses`

- `identifier` `(integer)`
  ICMP echo identifier for the ICMP envelope modes.
  Default: `44975` (`0xAFAF`)

- `sequence-start` `(integer)`
  Initial ICMP echo sequence counter for the ICMP envelope modes.
  Default: `0`

- `ipv4-id-start` `(integer)`
  Initial outer IPv4 identification counter for the ICMP envelope modes.
  Default: `0`

- `ttl` `(integer)`
  Default outer IPv4 TTL for the modes that create a fresh outer IPv4 header.
  Default: `64`

- `tos` `(integer)`
  Default outer IPv4 TOS byte for the modes that create a fresh outer IPv4 header.
  Default: `0`

- `xor-byte` `(integer)`
  XOR byte applied only to the ICMP payload in the ICMP envelope modes.

- `roundup-size` `(boolean)`
  Pads only the ICMP payload in the ICMP envelope modes.
  Default: `false`

- `source` `(string)`
  Required only when `strategy` is `wrap-in-new-ip-and-icmp-header`.
  Must be a single IPv4 address.

- `dest` `(string)`
  Required only when `strategy` is `wrap-in-new-ip-and-icmp-header`.
  Must be a single IPv4 address.

- `swap-protocol` `(string or integer)`
  Required only when `strategy` is `change-only-ipv4-protocol-number`.

## Example

```json
{
  "name": "icmp-client",
  "type": "PingClient",
  "settings": {
    "strategy": "wrap-in-new-ip-and-icmp-header",
    "identifier": 4660,
    "source": "198.51.100.10",
    "dest": "203.0.113.20",
    "xor-byte": 90,
    "roundup-size": true,
    "sequence-start": 0,
    "ttl": 64
  },
  "next": "raw-out"
}
```

## Notes

- `required_padding_left` remains `28` bytes so the tunnel can prepend the worst-case IPv4 plus ICMP envelope safely
- fragmented outer ICMP packets are not decapsulated here
- unmatched IPv4 traffic is still forwarded unchanged in the same direction
- only IPv6 is dropped unconditionally
- legacy aliases such as `warp-*`, `warp-in-icmp-header-and-update-ipv4-header`, `change-only-ip4-packet-identifier-number`, and `swap-identifier` are still accepted for backward compatibility
