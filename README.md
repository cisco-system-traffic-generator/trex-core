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

### Run

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

## Notes

- The current Docker Compose setup targets `linux/amd64` for predictable builds on both x86 and Apple Silicon hosts.
- Full DPDK NIC passthrough is intended for Linux hosts with the required device mounts and hugepage configuration.
- On macOS, the container is mainly useful for building, packaging, CLI access, and limited validation.

## Documentation

- TRex docs: https://trex-tgn.cisco.com/trex/doc/index.html
- Project wiki: https://github.com/cisco-system-traffic-generator/trex-core/wiki
