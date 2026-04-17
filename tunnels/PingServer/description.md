# PingServer Node

`PingServer` is the server-side peer for `PingClient`. On the upstream path it reverses the configured packet disguise logic, and on the downstream path it reapplies the matching strategy toward the client side.

It is a pure packet tunnel created with `packettunnelCreate()`, so it runs on the chain's worker packet lines and does not add per-line state.

## What It Does

- upstream applies the configured reverse transform for packets coming from the client side
- downstream applies the forward transform toward the client side
- only IPv4 is supported at runtime
- any IPv6 packet that reaches `PingServer` is logged and dropped
- `xor-byte` and `roundup-size` only affect the ICMP envelope modes
- `identifier`, `sequence-start`, and `ipv4-id-start` are only meaningful for the ICMP envelope modes

## `strategy`

### `warp-in-new-ip-and-icmp-header`

- upstream expects:
  `outer IPv4 header -> ICMP echo header -> original IPv4 packet`
- downstream recreates that same envelope
- requires both `source` and `dest` in `settings`
- verifies the configured outer source and destination before decapsulation
- if ICMP framing and identifier match but source or destination does not, `PingServer` logs a runtime warning and leaves the packet unchanged for the next node

### `warp-in-icmp-header-and-update-ipv4-header`

- still uses a full outer IPv4 packet plus ICMP header
- does not ask for `source` or `dest`
- keeps outer source and destination from the current IPv4 packet
- keeps the configured `ttl` and `tos` behavior of the existing tunnel
- still requires the recovered payload to be a valid IPv4 packet before decapsulation succeeds

Important note:
This mode still creates a fresh outer IPv4 header. That is necessary because the original IPv4 header must stay inside the ICMP payload so the peer can restore the original packet exactly.

### `warp-in-only-icmp-header`

- still produces an outer IPv4 plus ICMP envelope
- does not ask for `source` or `dest`
- keeps outer source and destination from the current IPv4 packet
- keeps configured `ttl` and `tos` for the new outer IPv4 header
- treats the recovered ICMP payload as opaque bytes and does not insist it is a valid IPv4 packet before passing it upstream

### `change-only-ip4-packet-identifier-number`

- does not add an ICMP header and does not prepend a new IPv4 header
- only swaps the IPv4 protocol number in place
- requires `swap-identifier`
- downstream changes packets whose current IPv4 protocol matches `swap-identifier` into `ICMP`
- upstream changes matching `ICMP` packets back to `swap-identifier`
- this mode does not use `identifier`, `sequence-start`, `ipv4-id-start`, `xor-byte`, or `roundup-size`

`swap-identifier` accepts:

- `"TCP"`
- `"UDP"`
- `"ICMP"`
- an integer protocol number between `0` and `255`

## Optional `settings`

- `strategy` `(string)`
  Controls packet transformation mode.
  Default: `warp-in-icmp-header-and-update-ipv4-header`

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
  Required only when `strategy` is `warp-in-new-ip-and-icmp-header`.
  Must be a single IPv4 address.

- `dest` `(string)`
  Required only when `strategy` is `warp-in-new-ip-and-icmp-header`.
  Must be a single IPv4 address.

- `swap-identifier` `(string or integer)`
  Required only when `strategy` is `change-only-ip4-packet-identifier-number`.

## Example

```json
{
  "name": "icmp-server",
  "type": "PingServer",
  "settings": {
    "strategy": "warp-in-new-ip-and-icmp-header",
    "identifier": 4660,
    "source": "203.0.113.20",
    "dest": "198.51.100.10",
    "xor-byte": 90,
    "roundup-size": true,
    "sequence-start": 0,
    "ttl": 64
  },
  "next": "tun-out"
}
```

## Notes

- `required_padding_left` remains `28` bytes so the tunnel can prepend the worst-case IPv4 plus ICMP envelope safely
- fragmented outer ICMP packets are not decapsulated here
- unmatched IPv4 traffic is still forwarded unchanged in the same direction
- only IPv6 is dropped unconditionally
