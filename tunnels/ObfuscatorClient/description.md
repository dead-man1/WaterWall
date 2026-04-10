# ObfuscatorClient Node

`ObfuscatorClient` applies a reversible payload transform to traffic passing through it. In the current implementation, the only supported method is XOR obfuscation.

In practice, this node is used together with `ObfuscatorServer` configured with the same method and key.

## What It Does

- Reads payload from the previous node.
- Applies the configured obfuscation method before sending payload to the next node.
- Applies the same operation again on downstream payload returning from the next node.
- Forwards all init, est, finish, pause, and resume events without changing their meaning.

Because XOR is symmetrical, the same transform is used in both directions.

## Typical Placement

A common layout is:

- normal payload-producing node before `ObfuscatorClient`
- `ObfuscatorClient`
- some transport path
- `ObfuscatorServer` on the remote side
- service-facing nodes after it

This tunnel is useful only as a lightweight payload obfuscation layer. It is not a secure encryption layer.

## Configuration Example

```json
{
  "name": "obfuscator-client",
  "type": "ObfuscatorClient",
  "settings": {
    "method": "xor",
    "xor_key": 90
  },
  "next": "transport-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"ObfuscatorClient"`.

- `next` `(string)`
  The next node that should receive the transformed payload.

### `settings`

- `method` `(string)`
  Obfuscation method to use.

  Currently supported values:
  - `"xor"`

- `xor_key` `(integer)`
  Required when `method` is `"xor"`.

  This value is converted to a single byte and used as the XOR key.

## Optional `settings` Fields

There are no additional tunnel-specific optional settings in the current implementation.

## Detailed Behavior

### XOR behavior

For each payload buffer:

- before sending upstream to the next node, every byte is XORed with `xor_key`
- when payload comes back downstream, every byte is XORed with the same `xor_key` again

Because XOR is reversible, both ends must use the same key.

### Event forwarding behavior

`ObfuscatorClient` does not change line ownership or connection topology.

It simply forwards:

- upstream `init`, `est`, `finish`, `pause`, `resume`
- downstream `init`, `est`, `finish`, `pause`, `resume`

Only payload is modified.

### Implementation details

The XOR helper is optimized for the current platform:

- AVX2 path when available
- 64-bit chunked path on aligned 64-bit builds
- 32-bit chunked or byte-by-byte fallback otherwise

That optimization affects speed only, not the visible behavior.

## Notes And Caveats

- `ObfuscatorClient` is intended to be paired with `ObfuscatorServer` using the same `method` and `xor_key`.
- Current support is limited to `method: "xor"`.
- This is obfuscation, not cryptographic security.
- `xor_key` is stored as a single byte in the current implementation, so values outside `0..255` are effectively truncated.
