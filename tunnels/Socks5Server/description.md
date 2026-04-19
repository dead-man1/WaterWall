# Socks5Server Node

`Socks5Server` is a server-side SOCKS5 middle tunnel for Waterwall.

It accepts SOCKS5 control traffic from its previous node, performs method negotiation and optional username/password
authentication, then either:

- opens a normal Waterwall upstream connection for `CONNECT`, or
- creates an authenticated UDP association for `UDP ASSOCIATE`

This tunnel is written to fit normal Waterwall chain rules:

- the previous node is usually a listener such as `TcpListener` or `UdpListener`
- the next node still performs the real outbound transport work
- line state is created during `Init`
- finishes destroy local state before propagating the real Waterwall close

## What It Does

- Implements SOCKS5 method negotiation.
- Supports optional username/password authentication.
- Supports `CONNECT`.
- Supports `UDP ASSOCIATE`.
- Rejects `BIND`.
- Holds TCP payload until the SOCKS5 `CONNECT` request is accepted.
- Authenticates UDP datagrams against a live TCP control connection.
- Creates internal backend UDP lines per requested remote destination.
- Wraps returned UDP payload back into SOCKS5 UDP datagrams.

## Typical Placement

TCP-only SOCKS5 server:

- `TcpListener -> Socks5Server -> TcpConnector`

SOCKS5 server with UDP support:

- `TcpListener -> Socks5Server -> TcpConnector`
- `UdpListener -> Socks5Server -> UdpConnector`

The same `Socks5Server` implementation handles both paths.

Important:

- `CONNECT` uses the TCP control line and forwards upstream through the normal next tunnel.
- `UDP ASSOCIATE` does not create a downstream TCP stream.
- UDP payload is only accepted when the sender matches an authenticated live TCP control association.

## Configuration Example

```json
{
  "name": "socks-server",
  "type": "Socks5Server",
  "settings": {
    "connect": true,
    "udp": true,
    "ipv4": "0.0.0.0",
    "users": [
      {
        "username": "alice",
        "password": "secret"
      }
    ],
    "verbose": false
  },
  "next": "next-node-name"
}
```

Single-account shorthand is also accepted:

```json
{
  "name": "socks-server",
  "type": "Socks5Server",
  "settings": {
    "connect": true,
    "udp": false,
    "username": "alice",
    "password": "secret"
  },
  "next": "next-node-name"
}
```

If you want no authentication at all, simply omit `username`, `password`, and `users`.

If you want username-based authentication with an empty password, that is also accepted:

```json
{
  "name": "socks-server",
  "type": "Socks5Server",
  "settings": {
    "username": "alice"
  },
  "next": "next-node-name"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"Socks5Server"`.

### `settings`

There are no always-required tunnel-specific settings.

However:

- at least one of `connect` or `udp` must be enabled
- when `udp` is enabled, `ipv4` is required

## Optional `settings` Fields

- `connect` `(boolean)`
  Enables SOCKS5 `CONNECT`.
  Default: `true`

- `udp` `(boolean)`
  Enables SOCKS5 `UDP ASSOCIATE`.
  Default: `false`

- `ipv4` `(string)`
  Required when `udp` is enabled.

  This is the IPv4 address placed in the SOCKS5 UDP associate reply.
  The reply port is not configured here. It is taken from the current TCP listener port that accepted the control
  connection.

  Example:

  - if the TCP control connection came in through local port `443`
  - and `ipv4` is `"0.0.0.0"`

  then the UDP associate reply is effectively:

  - `0.0.0.0:443`

- `username` and `password` `(string)`
  Single-account shorthand.

  `username` may be provided without `password`.
  In that case the configured password is the empty string.

  `password` may not be provided by itself.

- `users` `(array)`
  Multi-account form.

  Each entry must be an object with:

  - `username`
  - optional `password`

- `accounts`
  Alias for `users`.

- `verbose` `(boolean)`
  Enables extra debug logging.

## Detailed Behavior

### Control-line behavior

When a TCP line reaches `Socks5Server`:

- line state is initialized as a TCP control line
- the tunnel waits for the SOCKS5 greeting
- it selects either:
  - no-authentication, or
  - username/password authentication
- after successful authentication it waits for the SOCKS5 request

If no users are configured:

- the tunnel advertises and accepts SOCKS5 no-authentication mode

If users are configured:

- the tunnel requires SOCKS5 username/password authentication
- configured accounts may use an empty password

For `CONNECT`:

- the requested destination is copied into `line->routing_context.dest_ctx`
- the transport protocol is set to TCP
- upstream `Init` is sent to the next tunnel
- payload is buffered until downstream `Est` arrives
- only then does the tunnel send the SOCKS5 success reply and emit downstream `Est` toward the previous node

For `UDP ASSOCIATE`:

- no upstream transport line is created from the control line
- a UDP association is registered against the authenticated TCP control line
- the reply address uses `settings.ipv4`
- the reply port uses the local TCP listener port that accepted this control connection

### UDP association security model

This tunnel does not allow the UDP port to behave like an open proxy.

Current checks:

- the sender must match a registered UDP association
- that association must belong to a live TCP control line
- that TCP control line must still be authenticated

When the TCP control line closes:

- the UDP association is removed immediately
- later UDP packets from that sender are rejected

### UDP payload behavior

When an authenticated UDP datagram arrives:

- the tunnel validates the SOCKS5 UDP header
- fragmented SOCKS5 UDP packets (`FRAG != 0`) are ignored conservatively
- the requested destination is parsed from the UDP header
- a worker-local internal backend UDP line is created or reused for that destination
- the UDP payload body is sent upstream through that internal line

When a reply comes back from the next tunnel:

- the reply payload is wrapped into a SOCKS5 UDP response header
- the source address in that header is the remote destination represented by the backend UDP line
- the wrapped datagram is sent back toward the previous node

### Internal backend UDP lines

For UDP forwarding, `Socks5Server` creates normal Waterwall lines behind the UDP client side.

This is important for composability:

- the UDP listener line remains the client-facing association line
- per-remote outbound destinations get their own backend lines
- the packet line model is not abused as if it were a normal closable connection line

### Finish behavior

The implementation follows normal Waterwall finish ordering:

- local state is destroyed before propagating the real close
- UDP associations are unregistered before the control line is allowed to die
- internal UDP remote lines detach from their client line before being finished
- re-entrant callbacks are protected so the tunnel does not read line state after shutdown paths

## Notes And Caveats

- `BIND` is currently rejected with `command not supported`.
- SOCKS5 UDP fragmentation is not reassembled; packets with `FRAG != 0` are ignored.
- UDP support currently assumes your TCP and UDP listener topology is arranged so the returned TCP listener port is the
  correct SOCKS5 UDP port for the client to use.
- `required_padding_left` is set for the worst-case SOCKS5 UDP header so the tunnel can prepend UDP headers without
  breaking Waterwall buffer-padding assumptions.
