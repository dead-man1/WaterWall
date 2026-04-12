# IpManipulator Node

`IpManipulator` is a packet tunnel that mutates IPv4 packets in place.

It is meant for layer-3 chains where the payload is already a raw IP packet, not a normal TCP stream line.

The current implementation provides three classes of tricks:

- protocol-number swapping
- TLS ClientHello fragmentation and shuffle (`sni-blender`)
- TCP flag bit rewriting

## What It Does

- Reads raw packet payload on the upstream and downstream packet paths.
- Applies enabled packet tricks in place.
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
    "sni-blender": true,
    "sni-blender-packets": 4,
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

### TCP flag rewrite settings

The current implementation supports these key prefixes:

- `up-tcp-bit-...`
- `dw-tcp-bit-...`

Supported suffixes are:

- `cwr`
- `ece`
- `urg`
- `ack`
- `psh`
- `rst`
- `syn`
- `fin`

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

## Notes And Caveats

- This tunnel is for raw packet chains, not normal byte-stream chains.
- Only IPv4 packets are modified by the current implementation.
- `sni-blender` is upstream-only. The downstream half of that trick is currently a no-op.
- The tunnel relies on later packet-writing code to honor `line->recalculate_checksum` and rebuild packet checksums.
- `sni-blender-packets` is required when `sni-blender` is enabled.
- The struct contains `trick_sni_blender_packets_delay_max`, but current JSON parsing does not expose or use it.
