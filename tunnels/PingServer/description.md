# PingServer Node

`PingServer` is the server-side peer for `PingClient`. It unwraps matching IPv4 ICMP echo-request tunnel packets on the upstream path and re-wraps downstream IPv4 packets into fresh ICMP echo-request packets.

It is a pure packet tunnel created with `packettunnelCreate()`, so it runs on the chain's worker packet lines and does not add per-line state.

## What It Does

- upstream payload decapsulates:
  outer IPv4 header -> ICMP echo header -> original IPv4 packet
- downstream payload encapsulates raw IPv4 packets into real IPv4 ICMP echo-request packets
- outgoing server packets are also ICMP echo requests, never ICMP echo replies
- the configured ICMP identifier is matched before decapsulation
- `xor-byte` can XOR the ICMP payload bytes before server-side encapsulation and restore them during decapsulation
- `roundup-size` can add a 2-byte original-size prefix and pad the ICMP payload up to `64`, `128`, `256`, `512`, `1024`, or the maximum supported ICMP payload size
- outer IPv4 and ICMP checksums are calculated immediately when server-side encapsulation happens
- total packet size is capped at `1500` bytes, so oversized payloads are logged and dropped

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

- `xor-byte` `(integer)`
  XOR byte applied to the ICMP payload only.
  Omit this field to disable XOR.

- `roundup-size` `(boolean)`
  When `true`, the ICMP payload stores:
  `2-byte original-size -> original IPv4 packet -> random filler`
  and rounds the payload length up to the next supported bucket.
  Default: `false`

## Example

```json
{
  "name": "icmp-server",
  "type": "PingServer",
  "settings": {
    "local-ip": "203.0.113.20",
    "peer-ip": "198.51.100.10",
    "identifier": 4660,
    "xor-byte": 90,
    "roundup-size": true,
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
- with `roundup-size`, packets larger than `1470` bytes are dropped because the 2-byte size prefix must still fit inside the `1500` byte total packet limit
