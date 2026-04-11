# EncryptionClient

`EncryptionClient` applies framed AEAD encryption on upstream traffic and decrypts framed traffic on downstream.

Recommended chain usage:

- Client side: `TcpListener -> EncryptionClient -> TcpConnector`
- Server side counterpart: `TcpListener -> EncryptionServer -> TcpConnector`

## Settings

- `algorithm` (string or number, optional): `chacha20-poly1305` (default) or `aes-gcm`
- `password` (string, required): shared secret used for key derivation
- `salt` (string, optional): key-derivation salt, default `waterwall-encryption`
- `kdf-iterations` (number, optional): key derivation rounds, default `12000`
- `max-frame-size` (number, optional): maximum plaintext frame size in bytes, default `65535`

## Example

```json
{
  "name": "enc-client",
  "type": "EncryptionClient",
  "settings": {
    "algorithm": "chacha20-poly1305",
    "password": "replace-with-a-strong-secret",
    "salt": "chain-A",
    "kdf-iterations": 20000,
    "max-frame-size": 65535
  }
}
```
