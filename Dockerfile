FROM python:3.11-slim-bullseye AS builder

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
    mkdir -p /opt/trex && \
    rsync -aL \
    --exclude='bird/' \
    --exclude='*.pyc' \
    --exclude='__pycache__' \
    /src/trex-core/scripts/ /opt/trex/


FROM python:3.11-slim-bullseye AS runtime

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
COPY trex.yml /etc/trex_cfg.yaml

RUN mkdir -p /mnt/huge /tmp/trex-bird

LABEL org.opencontainers.image.title="TRex Core"
LABEL org.opencontainers.image.description="Container image for building and running TRex with Python 3.11"
LABEL org.opencontainers.image.source="https://github.com/cisco-system-traffic-generator/trex-core"
LABEL trex.run="docker run --rm -it --privileged --cap-add=ALL -v /mnt/huge:/mnt/huge -v /lib/modules:/lib/modules:ro -v /sys/bus/pci/devices:/sys/bus/pci/devices -v /sys/devices/system/node:/sys/devices/system/node -v /dev:/dev IMAGE"

ENTRYPOINT ["/bin/bash"]
CMD ["-l"]
