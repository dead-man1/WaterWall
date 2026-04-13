# PingServer Node

`PingServer` is the server-side peer for `PingClient`. It unwraps matching IPv4 ICMP echo-request tunnel packets on the upstream path and re-wraps downstream IPv4 packets into fresh ICMP echo-request packets.

It is a pure packet tunnel created with `packettunnelCreate()`, so it runs on the chain's worker packet lines and does not add per-line state.

## What It Does

- upstream payload decapsulates:
  outer IPv4 header -> ICMP echo header -> original IPv4 packet
- downstream payload encapsulates raw IPv4 packets into real IPv4 ICMP echo-request packets
- outgoing server packets are also ICMP echo requests, never ICMP echo replies
- the configured ICMP identifier is matched before decapsulation
- outer IPv4 and ICMP checksums are calculated immediately when server-side encapsulation happens

## Required `settings`

- `local-ip` `(string)`
  Outer IPv4 source address used when this server encapsulates downstream traffic.

- `peer-ip` `(string)`
  Outer IPv4 destination address used when this server encapsulates downstream traffic.

## Optional `settings`

- `identifier` `(integer)`
  ICMP echo identifier.
  Default: `44975` (`0xAFAF`)

- `sequence-start` `(integer)`
  Initial ICMP echo sequence number counter.
  Default: `0`

- `ipv4-id-start` `(integer)`
  Initial outer IPv4 identification counter.
  Default: `0`

- `ttl` `(integer)`
  Outer IPv4 TTL.
  Default: `64`

- `tos` `(integer)`
  Outer IPv4 TOS byte.
  Default: `0`

## Example

```json
{
  "name": "icmp-server",
  "type": "PingServer",
  "settings": {
    "local-ip": "203.0.113.20",
    "peer-ip": "198.51.100.10",
    "identifier": 4660,
    "sequence-start": 0,
    "ttl": 64
  },
  "next": "tun-out"
}
```

## Notes

- only IPv4 inner packets are encapsulated by this tunnel
- only matching IPv4 ICMP echo-request envelopes are decapsulated on the upstream path
- fragmented outer ICMP packets are not reassembled here, so they are passed through unchanged
