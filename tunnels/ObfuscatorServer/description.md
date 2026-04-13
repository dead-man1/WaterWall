# ObfuscatorServer Node

`ObfuscatorServer` is the server-side peer of `ObfuscatorClient`. It applies the same reversible payload transform on the remote side so payload is restored before reaching the next node and re-obfuscated again on the way back.

It is implemented as a stateless packet tunnel and does not keep per-line tunnel state.

In practice, this node is used together with `ObfuscatorClient` configured with the same method and key.

## What It Does

- Receives transformed payload from the previous node.
- Applies the configured method to recover the original payload before forwarding it to the next node.
- Applies the same method again to downstream reply payload before sending it back.
- Passes init, est, finish, pause, and resume events through unchanged.

In the current implementation, the only supported method is XOR.

## Typical Placement

A common layout is:

- transport-facing node before `ObfuscatorServer`
- `ObfuscatorServer`
- normal service-facing nodes after it

It should usually sit opposite `ObfuscatorClient`.

## Configuration Example

```json
{
  "name": "obfuscator-server",
  "type": "ObfuscatorServer",
  "settings": {
    "method": "xor",
    "xor_key": 90,
    "skip": "transport",
    "tls_record_header": true
  },
  "next": "service-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"ObfuscatorServer"`.

- `next` `(string)`
  The next node that should receive the restored payload.

### `settings`

- `method` `(string)`
  Obfuscation method to use.

  Currently supported values:
  - `"xor"`

- `xor_key` `(integer)`
  Required when `method` is `"xor"`.

  This value is converted to a single byte and used as the XOR key.

- `tls_record_header` `(boolean)`
  Optional.

  When `true`, the transport-facing payload is wrapped in a minimal TLS application-data record header:
  content type `23`, version `0x0303`, and a 2-byte payload length.

- `skip` `(string)`
  Optional.

  Controls which leading packet bytes are left unobfuscated before XOR runs.

  Supported values:
  - `"none"`: XOR the whole payload buffer
  - `"ipv4"`: if the payload starts with a valid IPv4 packet, leave only the IPv4 header clear
  - `"transport"`: if the payload starts with a valid IPv4 TCP/UDP packet, leave the IPv4 header and the TCP/UDP header clear

## Optional `settings` Fields

There are no additional tunnel-specific optional settings in the current implementation beyond `tls_record_header` and `skip`.

## Detailed Behavior

### XOR behavior

For each payload buffer:

- payload arriving from the previous node is XORed after the configured skip region and then forwarded upstream to the next node
- payload returning from the next node is XORed again after the configured skip region before being sent back to the previous node

Because XOR is symmetrical, the same code path is valid for obfuscation and de-obfuscation.

When `skip` is enabled and the payload does not parse as a valid IPv4 packet, the tunnel falls back to XORing the whole payload buffer.

### Optional TLS-like record header

When `tls_record_header` is enabled:

- before sending payload back toward the transport, the tunnel prepends a 5-byte TLS-like application-data header after XOR
- when payload arrives from the transport, it validates and strips that same 5-byte header before XOR
- the header bytes are `17 03 03` followed by the big-endian payload length

This only imitates the post-handshake TLS record shape. It is not a real TLS handshake or encryption layer.

### Event forwarding behavior

`ObfuscatorServer` forwards all non-payload events without adding protocol or buffering of its own:

- upstream `init`, `est`, `finish`, `pause`, `resume`
- downstream `init`, `est`, `finish`, `pause`, `resume`

Only payload bytes are transformed.

### Implementation details

Like the client side, the server-side XOR helper uses optimized chunked implementations when possible:

- AVX2 when available
- 64-bit aligned processing when possible
- 32-bit or byte-wise fallback otherwise

This is a performance detail only.

## Notes And Caveats

- `ObfuscatorServer` is intended to be paired with `ObfuscatorClient` using the same `method` and `xor_key`.
- Current support is limited to `method: "xor"`.
- This tunnel does not provide cryptographic protection.
- `xor_key` is stored as a single byte in the current implementation, so values outside `0..255` are effectively truncated.
- When `tls_record_header` is enabled, a single payload buffer must fit in one TLS-style record, so payloads larger than `65535` bytes are dropped.
