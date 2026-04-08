# TRex Core

TRex is a low-cost, high-speed traffic generator for stateful and stateless network testing.

It is built on DPDK and supports realistic L3-L7 traffic generation, Python automation, virtualization use cases, and interactive workflows.

## Highlights

- Stateful and stateless traffic generation
- DPDK-based high-performance packet processing
- Python automation APIs
- Interactive console support
- Virtualized and containerized deployment options
- Support for physical and virtual NICs

## Docker

This repository includes a Docker image and Compose setup for building and running TRex with Python 3.11.

### Build

```bash
docker compose build
```

### Open The Container

```bash
docker compose run --rm trex
```

This starts an interactive shell in the container.

Inside the container, TRex is available at `/opt/trex`.

Useful commands:

```bash
t-rex-64 --help
trex-console
```

## TRex Config

The Compose setup mounts the local config file:

```text
trex.yml
```

into the container at:

```text
/etc/trex_cfg.yaml
```

Edit the local file to change interfaces, IPs, and other TRex runtime settings.

Important fields in `trex.yml`:

- `interfaces`: PCI addresses of the NICs TRex should use
- `port_limit`: number of ports TRex should expose
- `c`: number of worker cores
- `port_info`: IP and gateway values per port

## Configure And Run TRex

### 1. Edit The Config

Update [trex.yml](/Users/anurag/SceptreAI/trex-core/trex.yml) before running TRex.

At minimum, review:

- `interfaces`
- `port_limit`
- `c`
- `port_info`

The sample values in the file are placeholders and usually need to be replaced with values that match your system.

### 2. Build The Container

```bash
docker compose build
```

### 3. Start An Interactive Shell

```bash
docker compose run --rm trex
```

### 4. Verify The Runtime Inside The Container

```bash
cd /opt/trex
t-rex-64 --help
trex-console
```

### 5. Run TRex Server

For interactive stateless mode:

```bash
cd /opt/trex
t-rex-64 --stl -i
```

For interactive ASTF mode:

```bash
cd /opt/trex
t-rex-64 --astf -i
```

For a basic profile run:

```bash
cd /opt/trex
t-rex-64 -f cap2/dns.yaml -m 10 -d 20
```

TRex expects to run as root. In this container, that is already the default.

### 6. Use The Console

In another shell inside the same container, or in a second `docker compose run --rm trex` session if appropriate for your workflow:

```bash
cd /opt/trex
trex-console
```

The console is the easiest way to interact with a running TRex instance in interactive mode.

## Python Scripts And Examples

This repository also contains Python-based tooling and examples.

### Console Launcher

The `trex-console` command is a Python entrypoint that loads the interactive control-plane package from:

```text
scripts/automation/trex_control_plane/interactive
```

### Example Python Programs

Useful example directories:

- `scripts/automation/trex_control_plane/interactive/trex/examples/stl`
- `scripts/automation/trex_control_plane/interactive/trex/examples/astf`
- `scripts/automation/trex_control_plane/interactive/trex/examples/emu`
- `scripts/stl`
- `scripts/emu`

Examples you may want to inspect first:

- `scripts/automation/trex_control_plane/interactive/trex/examples/stl/stl_simple_burst.py`
- `scripts/automation/trex_control_plane/interactive/trex/examples/stl/stl_imix.py`
- `scripts/automation/trex_control_plane/interactive/trex/examples/astf/astf_example.py`
- `scripts/automation/trex_control_plane/interactive/trex/examples/emu/emu_arp.py`
- `scripts/stl/udp_1pkt_simple.py`

### Running A Python Example

Most Python examples are intended to be run from inside `/opt/trex`, where the packaged scripts and Python paths are available.

A typical pattern looks like:

```bash
cd /opt/trex
python3 automation/trex_control_plane/interactive/trex/examples/stl/stl_simple_burst.py
```

Some examples assume that the TRex server is already running and reachable.

## Notes

- The current Docker Compose setup targets `linux/amd64` for predictable builds on both x86 and Apple Silicon hosts.
- Full DPDK NIC passthrough is intended for Linux hosts with the required device mounts and hugepage configuration.
- On macOS, the container is mainly useful for building, packaging, CLI access, and limited validation.

## Documentation

- TRex docs: https://trex-tgn.cisco.com/trex/doc/index.html
- Project wiki: https://github.com/cisco-system-traffic-generator/trex-core/wiki
