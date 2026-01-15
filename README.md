# ft_ping

Educational implementation of a **`ping`-like** tool (ICMP Echo Request / Echo Reply) written in C.

This repository provides:

- a **`ft_ping`** binary buildable with `make`
- a ready-to-use **Docker** environment (including the **NET_RAW** capability required for ICMP raw sockets)
- argument parsing based on **argp**, with options close to the standard `ping`

---

## Prerequisites

### Linux (recommended)

- `make`
- a C compiler (`cc` / `gcc`)
- sufficient permissions to open **raw sockets** (see **Permissions**)

### Docker (optional, but convenient)

A `Dockerfile` and a `docker-compose.yml` are included to build and run the project inside a Debian container.

---

## Build

From the project root:

```bash
make
```

The resulting binary is:

```bash
./ft_ping
```

### Debug build

The Makefile provides a `debug` target enabling debug symbols and sanitizers:

```bash
make debug
```

---

## Usage

Basic usage:

```bash
./ft_ping <host>
```

Examples:

```bash
./ft_ping 8.8.8.8
./ft_ping google.com
```

### Supported options (argp)

The argument parser currently supports:

- `-v`, `--verbose` : verbose output
- `-c COUNT`, `--count=COUNT` : stop after **COUNT** replies
- `-w DEADLINE`, `--deadline=DEADLINE` : exit after **DEADLINE** seconds
- `-t TTL`, `--ttl=TTL` : set IP TTL (default: 64)
- `-i INTERVAL`, `--interval=INTERVAL` : seconds between packets (default: 1)
- `-s PACKETSIZE`, `--packetsize=PACKETSIZE` : number of payload bytes (default: 56)

Example:

```bash
./ft_ping -c 5 -i 0.2 8.8.8.8
```

---

## Permissions (CAP_NET_RAW)

Sending ICMP packets with a **raw socket** generally requires elevated privileges.

### Option 1 — Run as root (simple, but not recommended)

```bash
sudo ./ft_ping 8.8.8.8
```

### Option 2 — Grant `cap_net_raw` to the binary (recommended on Linux)

The Makefile attempts to apply this capability automatically after linking:

```makefile
setcap cap_net_raw+ep ft_ping
```

If needed, you can apply it manually:

```bash
sudo setcap cap_net_raw+ep ./ft_ping
```

You can check capabilities with:

```bash
getcap ./ft_ping
```

> Note: capabilities depend on your filesystem and distribution policies.

### Option 3 — Use Docker (recommended for a clean environment)

The provided compose configuration adds the required capability:

```yaml
cap_add:
  - NET_RAW
```

---

## Docker workflow

### Using `make` helpers

The Makefile includes Docker shortcuts:

```bash
make docker-build
make docker-up
make docker-enter
```

Also you can directly run:

```bash
make docker-enter
```

Once inside the container:

```bash
make
./ft_ping 8.8.8.8
```

### Using `docker compose` directly

```bash
docker compose build
docker compose up -d
docker compose exec ft_ping bash
```

---

## Debugging / Capturing ICMP Traffic with `tcpdump`

You can capture ICMP packets on **localhost** (loopback) into a `.pcap` file:

```bash
tcpdump -i lo -nn -s 0 -w ping-localhost.pcap icmp and host 127.0.0.1
```

You can also capture ICMP traffic on a **network interface** such as `eth0` (useful to observe real network traffic, not only local loopback):

```bash
tcpdump -i eth0 -nn -s 0 -w ping-eth0.pcap icmp
```

### What `-i` does

Specifies the **network interface** to capture packets from:

* `-i lo` captures traffic on the **loopback** interface (localhost: `127.0.0.1`)
* `-i eth0` captures traffic on the **Ethernet** interface (real network traffic)

You can list available interfaces with:

```bash
tcpdump -D
```

### What `-nn` does

Disables name resolution:

* `-n` : do **not** resolve IP addresses to DNS names
* `-nn` : additionally, do **not** resolve ports to service names

This keeps output purely numeric and avoids extra DNS noise during diagnostics.

### What `-s 0` does

Controls the snapshot length (**snaplen**), i.e. the maximum number of bytes captured per packet.

* a small `-s` value truncates packets
* `-s 0` means “capture the entire packet” (useful maximum snaplen)

This is a good default when generating `.pcap` files for inspection in Wireshark.


---

## References

- ICMP (RFC 792): https://www.rfc-editor.org/rfc/rfc792
- GNU inetutils: https://www.gnu.org/software/inetutils/manual/
