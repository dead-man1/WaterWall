# TunDevice Node

`TunDevice` attaches WaterWall to a TUN interface. It reads IP packets from the virtual network device and forwards them into the chain, and it can also write IP packets from the chain back into the TUN device.

This node is a layer-3 adapter rather than a connection-oriented tunnel.

## What It Does

- Creates and configures a TUN interface.
- Assigns an IP address and subnet mask to that device.
- Brings the device up.
- Reads packets from the device and forwards them to the adjacent chain side.
- Writes packets received from the chain back into the device.
- Recalculates checksums before writing when the line requests it.

Because it is a packet adapter, this node does not create per-connection lines. It uses worker packet lines provided by the chain.

## Typical Placement

`TunDevice` can be placed at either edge of a chain:

- if it is last in the chain, packets read from the TUN device are forwarded to the previous node
- otherwise, packets read from the TUN device are forwarded to the next node

Payload coming from either side and reaching `TunDevice` is written into the TUN interface.

## Configuration Example

```json
{
  "name": "tun0-adapter",
  "type": "TunDevice",
  "settings": {
    "device-name": "tun0",
    "device-ip": "10.10.0.1/24",
    "device-mtu": 1500
  },
  "next": "next-node-name"
}
```

## Required JSON Fields

### Top-level fields

- `name` `(string)`
  A user-chosen name for this node.

- `type` `(string)`
  Must be exactly `"TunDevice"`.

### `settings`

- `device-name` `(string)`
  Name of the TUN interface to create or open.

- `device-ip` `(string)`
  Interface IP and subnet in CIDR form.
  Example: `"10.10.0.1/24"`

  The current implementation splits this into:
  - device IP address
  - subnet mask length

## Optional `settings` Fields

- `device-mtu` `(integer)`
  MTU to configure on the TUN device.

  Default: global MTU size used by WaterWall.

## Detailed Behavior

### Device setup

During `onPrepare`, `TunDevice`:

- decides which adjacent tunnel should receive packets read from the device
- creates the TUN device
- assigns the configured IP/subnet
- brings the device up

The actual device creation is deferred until prepare time because the tunnel needs chain context to know which side should receive packets.

### Packet input path

When the TUN device produces a packet:

- the packet is received on a worker
- the tunnel validates the packet format
- only IPv4 packets are currently accepted by this path
- the packet is forwarded through the chosen adjacent tunnel using the worker's packet line

If the device is down, the packet is dropped.

### Packet output path

When payload reaches `TunDevice` from upstream or downstream:

- the payload is treated as an IP packet
- checksum recalculation is performed if the line requests it
- the packet is written to the TUN device

Both upstream and downstream payload handlers write to the same TUN device.

### Checksum behavior

Before writing a packet, `TunDevice` checks line flags:

- if `recalculate_checksum` is set, the packet checksum is recomputed
- full packet checksum recalculation is attempted
- for fragmented IPv4 packets, transport checksum recalculation is skipped automatically and only the IP header checksum is recomputed

### Callback behavior

Most connection-style callbacks such as `init`, `est`, `finish`, `pause`, and `resume` are ignored by this adapter. The important behavior is packet read and packet write.

## Notes And Caveats

- The current receive path only forwards IPv4 packets.
- `device-ip` must be a valid CIDR string.
- On Windows, the implementation requires administrative privileges to load and manage the tunnel driver.
- Platform support depends on build and operating system support for TUN devices.
