# HttpClient Node

`HttpClient` is a stream tunnel that wraps Waterwall payload into an HTTP request on the upstream side and unwraps the HTTP response body on the downstream side.

It can speak HTTP/1.1, direct HTTP/2, or start as HTTP/1.1 and upgrade to `h2c`.

This node only formats HTTP. It does not create TLS by itself, so if you really want HTTPS on the wire you normally place `TlsClient` after it.

## What It Does

- Builds an HTTP request from tunnel configuration.
- Sends request headers during `Init`.
- Encodes upstream payload as HTTP request body.
- Decodes downstream HTTP response framing and forwards only the response body.
- Supports HTTP/1.1 chunked transfer for request bodies.
- Supports a single HTTP/2 stream per Waterwall line.
- Can attempt HTTP/1.1 to HTTP/2 cleartext upgrade with `Upgrade: h2c`.

This tunnel consumes HTTP headers internally. The previous tunnel sees body payload and lifecycle events, not raw HTTP header blocks.

## Typical Placement

A common layout is:

- some payload-producing tunnel
- `HttpClient`
- optional `TlsClient`
- `TcpConnector`

Examples:

- cleartext HTTP: `SomeTunnel -> HttpClient -> TcpConnector`
- HTTPS-like transport: `SomeTunnel -> HttpClient -> TlsClient -> TcpConnector`

## Configuration Example

```json
{
  "name": "http-client",
  "type": "HttpClient",
  "settings": {
    "host": "example.com",
    "path": "/api/upload",
    "scheme": "https",
    "port": 443,
    "method": "POST",
    "http-version": "both",
    "upgrade": true,
    "content-type": "application/json",
    "headers": {
      "x-client-name": "waterwall",
      "x-mode": "demo"
    }
  },
  "next": "tls-or-transport-node"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"HttpClient"`.

### `settings`

- `host` `(string)`
  Required request host name.

  This value is used for:
  - the HTTP `Host` header in HTTP/1.1
  - the `:authority` pseudo-header in HTTP/2

## Optional `settings` Fields

- `path` `(string)`
  Request path.
  Default: `"/"`

- `scheme` `(string)`
  Request scheme used in HTTP metadata.
  Default: `"https"`

  Current implementation note:
  this does not enable TLS by itself. It only affects HTTP metadata and the default port selection.

- `port` `(integer)`
  Request port.

  Default:
  - `443` when `scheme` is `https`
  - `80` otherwise

- `method` `(string)`
  HTTP method.
  Default: `"POST"`

  It must be one of Waterwall's built-in HTTP methods such as:
  - `GET`
  - `POST`
  - `PUT`
  - `HEAD`
  - `PATCH`
  - `DELETE`

- `http-version` `(number or string)`
  Selects request protocol mode.

  Supported values:
  - `1`
  - `2`
  - `"1.1"`
  - `"http1"`
  - `"http1.1"`
  - `"2"`
  - `"http2"`
  - `"h2"`
  - `"both"`
  - `"auto"`
  - `"1.1+2"`

  Current default: HTTP/2 mode.

- `upgrade` `(boolean)`
  Only meaningful when `http-version` is `both`.

  If enabled, the tunnel starts with an HTTP/1.1 request containing:
  - `Connection: Upgrade, HTTP2-Settings`
  - `Upgrade: h2c`
  - `HTTP2-Settings: ...`

  Default:
  - `true` when `http-version` is `both`
  - `false` otherwise

- `content-type` `(string)`
  Optional content type to emit automatically.

  It is matched against Waterwall's internal content-type table.
  Common accepted examples include:
  - `application/json`
  - `text/plain`
  - `application/octet-stream`
  - `multipart/form-data`

- `headers` `(object)`
  Extra request headers.

  Each key is the header name and each value must be a string.

## Detailed Behavior

### Lifecycle behavior

On upstream `Init`, `HttpClient`:

- allocates per-line HTTP state
- forwards upstream `Init` to the next tunnel
- immediately starts request setup based on the selected HTTP mode

The exact startup behavior is:

- HTTP/1.1 mode:
  send request headers right away
- HTTP/2 mode:
  create an nghttp2 client session, submit SETTINGS and request headers, then send the resulting frames upstream
- both plus upgrade:
  send an HTTP/1.1 upgrade request first and wait to see whether the server replies with `101 Switching Protocols`

### HTTP/1.1 request body behavior

For HTTP/1.1, request bodies are always sent as chunked transfer encoding.

That means:

- request headers include `Transfer-Encoding: chunked`
- each upstream payload buffer becomes one or more chunked body pieces
- upstream `Finish` sends the final `0\r\n\r\n` chunk

The implementation uses `required_padding_left = 16` so it can prepend chunk prefixes or HTTP/2 frame headers efficiently with `sbufShiftLeft()` when possible.

### HTTP/2 behavior

For HTTP/2, `HttpClient` uses nghttp2 and opens a single request stream per Waterwall line.

Current session settings include:

- `MAX_CONCURRENT_STREAMS = 1`
- `INITIAL_WINDOW_SIZE = 1 MiB`
- `MAX_FRAME_SIZE = 32 KiB`

Upstream payload is turned into HTTP/2 DATA frames.
If the remote peer advertises a smaller frame size, payload is split accordingly.

Upstream `Finish` becomes an empty DATA frame with `END_STREAM`, or `END_STREAM` is attached to the last DATA frame when appropriate.

### Upgrade behavior

When `http-version` is `both` and `upgrade` is enabled:

- the tunnel begins with HTTP/1.1
- the request advertises `Upgrade: h2c`
- upstream request payload is buffered until the protocol choice is known

If the response is:

- `101 Switching Protocols` with `Upgrade: h2c`
  the tunnel creates an nghttp2 session, applies the advertised `HTTP2-Settings`, and continues as HTTP/2

- a normal non-`101` response
  the tunnel stays in HTTP/1.1 mode and flushes buffered request payload as chunked body data

Unexpected `101` replies are treated as errors.

### Downstream response handling

For HTTP/1.1 responses, `HttpClient` parses headers internally and then forwards only the response body.

Supported response body modes are:

- chunked transfer encoding
- fixed `Content-Length`
- body-until-close
- no body for status codes that forbid one

Special cases handled in the code:

- `1xx` informational responses are skipped
- `204` and `304` are treated as bodyless
- `HEAD` requests are treated as bodyless on response

For HTTP/2 responses:

- HEADERS are consumed internally
- DATA frames are forwarded downstream as plain body payload
- `END_STREAM` becomes downstream `Finish`

## Notes And Caveats

- `HttpClient` does not provide encryption. Pair it with `TlsClient` if you need TLS on the wire.
- Response headers and status are not exposed to the previous tunnel. Only body payload and finish events are forwarded.
- Extra headers in `headers` are appended to HTTP/1.1 requests as-is.
- In HTTP/2 mode, extra headers whose names start with `:` are ignored.
- Automatic `Content-Type` emission only works for content types recognized by Waterwall's internal table. For custom values, add the header manually in `headers`.
- Current implementation is single-stream per line, not a general multi-stream HTTP/2 multiplexer.
