FROM ubuntu:19.04 as builder

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc g++ python zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

COPY . /src/trex-core
WORKDIR /src/trex-core

RUN cd ./linux_dpdk && \
    ./b configure --no-mlx=NO_MLX && \
    ./b build && \
    cd ../ && \
    cd ./linux && \
    ./b configure && \
    ./b build && \
    cd ../ && \
    rm -rf scripts/*.txt && \
    rm -rf scripts/_t-rex-64* && \
    rm -rf scripts/bp-sim* && \
    cp linux_dpdk/build_dpdk/linux_dpdk/_t-rex-64* scripts/ && \
    cp linux/build/linux/bp-sim* scripts/ && \
    rm -rf scripts/so/lib*.so && \
    cp linux_dpdk/build_dpdk/linux_dpdk/lib* scripts/so/

FROM ubuntu:19.04

RUN apt-get update && \
    apt-get install -y sudo python git zlib1g-dev pciutils vim valgrind \
                       strace wget curl iproute2 iptables tcpdump kmod \
                       iputils-arping iputils-clockdiff iputils-ping \
                       iputils-tracepath inetutils-traceroute mtr && \
    rm -rf /var/lib/apt/lists/*

COPY docker/bin/entrypoint.sh /usr/bin/entrypoint.sh
COPY docker/etc/trex_cfg.yaml /etc/trex_cfg.yaml
COPY --from=builder /src/trex-core/scripts /src/trex-core/scripts

WORKDIR /src/trex-core/scripts

ENTRYPOINT ["/usr/bin/entrypoint.sh"]
CMD [ "/bin/bash" ]
