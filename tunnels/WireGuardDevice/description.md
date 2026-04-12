`WireGuardDevice` is a packet tunnel that encrypts inner IP packets on the upstream side and decrypts WireGuard transport/handshake packets on the downstream side.

Current behavior:
- Supports multiple peers instead of a single hard-coded peer.
- Routes outbound packets with longest-prefix `AllowedIPs` matching, which is the cryptokey-routing rule WireGuard expects.
- Validates inbound decrypted packets against the peer's allowed source prefixes before passing plaintext back to the previous packet tunnel.
- Accepts comma-separated IPv4 and/or IPv6 CIDRs in `allowedips`.
- Accepts `endpoint` in `host:port` form for IPv4/domain names and `[ipv6]:port` form for IPv6 literals.
- Accepts optional `presharedkey` as a base64-encoded 32-byte key. If omitted, the peer runs without PSK.

Example settings:

```json
{
  "privatekey": "<base64-private-key>",
  "peers": [
    {
      "publickey": "<base64-peer-public-key>",
      "allowedips": "10.0.0.2/32,10.10.0.0/16,fd00::2/128",
      "endpoint": "vpn.example.com:51820",
      "persistentkeepalive": 25
    }
  ]
}
```
