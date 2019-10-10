FROM ubuntu:18.04
MAINTAINER me@mehthehedgehog.be

RUN apt-get update && \
    apt-get install -y sudo gcc g++ python git zlib1g-dev pciutils \
                       vim kmod strace wget curl iproute2 tcpdump \
                       iputils-arping iputils-clockdiff iputils-ping \
                       iputils-tracepath inetutils-traceroute mtr && \
    rm -rf /var/lib/apt/lists/*

COPY . /src/trex-core
WORKDIR /src/trex-core

RUN mv ./docker/bin/entrypoint.sh /usr/bin/entrypoint.sh && \
    mv ./docker/etc/trex_cfg.yaml /etc/trex_cfg.yaml

RUN cd ./linux_dpdk && \
    ./b configure && \
    ./b build

WORKDIR /src/trex-core/scripts

ENTRYPOINT ["/usr/bin/entrypoint.sh"]
CMD [ "/bin/bash" ]
