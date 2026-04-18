# IpManipulator Node

`IpManipulator` is a packet tunnel that mutates IPv4 packets in place.

It is meant for layer-3 chains where the payload is already a raw IP packet, not a normal TCP stream line.

The current implementation provides these classes of tricks:

- protocol-number swapping
- TLS ClientHello echo (`EchoSNI`)
- TLS ClientHello fragmentation and shuffle (`sni-blender`)
- TCP flag bit rewriting
- final-packet duplication

## What It Does

- Reads raw packet payload on the upstream and downstream packet paths.
- Applies enabled packet tricks in place.
- Optionally duplicates the final outgoing packet after all other enabled tricks.
- Marks packets for checksum recalculation when it changes protocol or TCP flags.
- Can replace one outgoing TLS ClientHello packet with multiple shuffled IP fragments.

This is a packet tunnel created with `packettunnelCreate()`, so normal stream-style `Init` and `Finish` callbacks are not part of its intended usage.

## Typical Placement

`IpManipulator` belongs in raw-packet chains, for example between packet-oriented nodes such as:

- `TunDevice`
- `WireGuardDevice`
- `RawSocket`
- other layer-3 packet tunnels

Typical use cases include:

- changing protocol numbers to evade simple filtering
- sending a crafted TLS ClientHello copy before the real ClientHello
- fragmenting a TLS ClientHello to alter packet shape
- testing how a path behaves when TCP control bits are rewritten

## Configuration Example

```json
{
  "name": "ip-manipulator",
  "type": "IpManipulator",
  "settings": {
    "protoswap-tcp": 253,
    "protoswap-tcp-2": 254,
    "protoswap-udp": 252,
    "first-sni": "cover.example.net",
    "echo-sni-count": 3,
    "echo-sni-replay-delay": 20,
    "echo-sni-final-delay": 50,
    "echo-sni-ttl": 1,
    "echo-sni-random-tcp-sequence": true,
    "sni-blender": true,
    "sni-blender-packets": 4,
    "packet-duplicate": 2,
    "up-tcp-bit-psh": "off",
    "up-tcp-bit-ack": "toggle",
    "dw-tcp-bit-syn": "packet->ack"
  },
  "next": "next-packet-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"IpManipulator"`.

### `settings`

At least one trick must be enabled.

If none of the supported trick settings are present, tunnel creation fails with:

- `IpManipulator: no tricks are enabled, nothing to do`

## Optional `settings` Fields

### Protocol-swap settings

- `protoswap` `(integer)`
  Alias for `protoswap-tcp`.

- `protoswap-tcp` `(integer)`
  Replacement IP protocol number for TCP packets.

- `protoswap-tcp-2` `(integer)`
  Optional second replacement protocol number for TCP.

  When set, upstream and downstream TCP packets alternate between the two configured replacement numbers independently per direction.

- `protoswap-udp` `(integer)`
  Replacement IP protocol number for UDP packets.

### SNI blender settings

- `sni-blender` `(boolean)`
  Enables the TLS ClientHello fragmentation trick.

- `sni-blender-packets` `(integer)`
  Required when `sni-blender` is enabled.

  Valid range in the current implementation:
  - `1` to `16`

### Packet duplication settings

- `packet-duplicate` `(integer)`
  Optional.

  Duplicates each final outgoing packet this many times, then sends the original packet once.

  This is applied as the last step of `IpManipulator`, after all other enabled tricks have finished shaping the packet.

### EchoSNI settings

- `first-sni` `(string)`
  Enables the EchoSNI trick and sets the SNI that will be written into the crafted TLS ClientHello copy.

- `echo-sni-ttl` `(integer)`
  Optional.

  When present, the crafted EchoSNI packet is sent with this IPv4 TTL value.

- `echo-sni-count` `(integer)`
  Optional.

  Number of crafted EchoSNI packets to send before the original ClientHello.

  Defaults to `1`.

- `echo-sni-replay-delay` `(integer)`
  Optional.

  Delay in milliseconds between crafted EchoSNI replays after the first one.

  Defaults to `0`.

  This value only matters when `echo-sni-count` is greater than `1`.

- `echo-sni-final-delay` `(integer)`
  Optional.

  Delay in milliseconds between the last crafted EchoSNI packet and the original ClientHello.

  Defaults to `0`.

- `echo-sni-random-tcp-sequence` `(boolean)`
  Optional.

  When `true`, the crafted EchoSNI packet gets a fresh random TCP sequence number before it is sent.

  When `false` or omitted, the crafted EchoSNI packet keeps the original TCP sequence number.

### TCP flag rewrite settings

The current implementation supports these key prefixes:

- `up-tcp-bit-...`
- `dw-tcp-bit-...`

Supported suffixes are:

- `"up-tcp-bit-cwr":"off",`
- `"up-tcp-bit-ece":"off",`
- `"up-tcp-bit-urg":"off",`
- `"up-tcp-bit-ack":"off",`
- `"up-tcp-bit-psh":"off",`
- `"up-tcp-bit-rst":"off",`
- `"up-tcp-bit-syn":"on",`
- `"up-tcp-bit-fin":"off"`

Example keys:

- `up-tcp-bit-ack`
- `up-tcp-bit-fin`
- `dw-tcp-bit-psh`
- `dw-tcp-bit-rst`

Supported values are:

- `off`
- `on`
- `toggle`
- `flip`
- `switch`
- `packet->cwr`
- `packet->ece`
- `packet->urg`
- `packet->ack`
- `packet->psh`
- `packet->rst`
- `packet->syn`
- `packet->fin`

`flip` and `switch` are accepted as aliases for `toggle`.

- `bit-transport` `(boolean)`
  Optional.

  When `true`, directions with configured TCP-bit rewrite actions append the original TCP flags byte to the end of the TCP transport payload before rewriting flags.

  Directions with no TCP-bit rewrite actions treat that final payload byte as the transported original flags, restore the TCP flags from it, and shrink the packet by one byte.

## Detailed Behavior

### Packet model

`IpManipulator` only touches packet payload callbacks:

- upstream packet payload goes through `ipmanipulatorUpStreamPayload()`
- downstream packet payload goes through `ipmanipulatorDownStreamPayload()`

Normal stream-style callbacks such as `Init` and `Finish` are intentionally not supposed to run for this tunnel.

### Protocol-number swap

The protocol-swap trick only applies to IPv4 packets.

Behavior:

- if the packet protocol is TCP and `protoswap-tcp` is enabled, the tunnel rewrites the IP protocol field to the configured custom number
- if the packet protocol is already equal to that custom number, it rewrites it back to normal TCP
- the same idea applies to `protoswap-udp`

If `protoswap-tcp-2` is configured:

- TCP packets alternate between `protoswap-tcp` and `protoswap-tcp-2`
- upstream and downstream maintain their own toggle state

Whenever the tunnel changes the protocol field, it sets `line->recalculate_checksum = true` so a later packet writer can rebuild checksums.

### SNI blender

Despite the name, this trick does not rewrite the TLS SNI string itself.

What it actually does is:

- detect an upstream IPv4 TCP packet carrying a TLS ClientHello
- split the IP payload into multiple IP fragments
- shuffle those fragments into random send order
- send the crafted fragments instead of the original packet

Important details from the current code:

- only upstream traffic is affected
- only IPv4 is supported
- only TCP packets are inspected
- only TLS ClientHello packets are fragmented
- already fragmented packets are skipped
- fragment count comes from `sni-blender-packets`
- fragment offsets are rounded to 8-byte boundaries as required by IP fragmentation

Before crafting fragments, the tunnel applies any pending checksum recalculation on the original packet, then marks each crafted packet for checksum recalculation before forwarding.

### EchoSNI

This trick is upstream-only and only applies to IPv4 TCP packets that begin with a TLS ClientHello carrying an SNI extension.

Behavior:

- detect an upstream TLS ClientHello
- parse the first host-name entry in the TLS server-name extension
- clone the packet and replace only the copied packet's SNI with `first-sni`
- send the modified copy first
- if `echo-sni-ttl` is set, update the crafted packet TTL to that value
- if `echo-sni-random-tcp-sequence` is `true`, randomize the crafted packet's TCP sequence number
- recompute the crafted packet checksum before send
- then forward the original packet using the original `line->recalculate_checksum` intent

If `first-sni` is longer or shorter than the original SNI, the copied packet updates the relevant TLS and IPv4 length fields.

### TCP flag rewriting

The TCP-bit trick only applies to valid IPv4 TCP packets.

For each configured bit action, the tunnel can:

- force the bit off
- force the bit on
- toggle it
- copy the value of another TCP flag from the same packet

If any flag changes:

- the TCP flags byte is rewritten
- `line->recalculate_checksum` is set to `true`

This happens independently on upstream and downstream using the `up-...` and `dw-...` setting families.

If `bit-transport` is enabled:

- rewrite directions append one extra payload byte carrying the original TCP flags before applying any configured TCP-bit actions
- restore directions copy that byte back into the TCP flags field and reduce the IPv4 packet length by one byte
- fragmented IPv4 packets are skipped so the tunnel only operates on whole TCP packets with a real TCP header and transport payload

## Notes And Caveats

- This tunnel is for raw packet chains, not normal byte-stream chains.
- Only IPv4 packets are modified by the current implementation.
- `EchoSNI` is upstream-only and rewrites the first TLS host-name entry in the crafted copy.
- `sni-blender` is upstream-only. The downstream half of that trick is currently a no-op.
- The tunnel relies on later packet-writing code to honor `line->recalculate_checksum` and rebuild packet checksums.
- `sni-blender-packets` is required when `sni-blender` is enabled.
- `echo-sni-random-tcp-sequence` affects only the crafted EchoSNI copy, not the original packet.
- The struct contains `trick_sni_blender_packets_delay_max`, but current JSON parsing does not expose or use it.
