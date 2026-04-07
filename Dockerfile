FROM python:3.14-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    file \
    g++ \
    gcc \
    git \
    libelf-dev \
    libnuma-dev \
    libpcap-dev \
    make \
    pkg-config \
    rsync \
    zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src/trex-core

COPY . .

WORKDIR /src/trex-core/linux_dpdk

RUN ./b configure && \
    ./b build && \
    ./b pkg --pkg-dir /tmp --pkg-file trex.tar.gz && \
    mkdir -p /opt/trex && \
    tar -xzf /tmp/trex.tar.gz -C /opt/trex --strip-components=1


FROM python:3.14-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV TREX_HOME=/opt/trex
ENV PATH=/opt/trex:$PATH

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    iproute2 \
    kmod \
    libelf1 \
    libnuma1 \
    libpcap0.8 \
    pciutils \
    procps \
    strace \
    sudo \
    vim-tiny \
    zlib1g \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/trex

COPY --from=builder /opt/trex/ /opt/trex/
COPY docker/ubuntu/16.04/trex_cfg.yaml /etc/trex_cfg.yaml

RUN mkdir -p /mnt/huge /tmp/trex-bird

LABEL org.opencontainers.image.title="TRex Core"
LABEL org.opencontainers.image.description="Container image for building and running TRex with Python 3.14"
LABEL org.opencontainers.image.source="https://github.com/cisco-system-traffic-generator/trex-core"
LABEL trex.run="docker run --rm -it --privileged --cap-add=ALL -v /mnt/huge:/mnt/huge -v /lib/modules:/lib/modules:ro -v /sys/bus/pci/devices:/sys/bus/pci/devices -v /sys/devices/system/node:/sys/devices/system/node -v /dev:/dev IMAGE"

ENTRYPOINT ["/opt/trex/t-rex-64"]
CMD ["--help"]
