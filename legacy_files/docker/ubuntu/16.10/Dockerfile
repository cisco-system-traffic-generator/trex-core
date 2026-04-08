FROM ubuntu:16.10
MAINTAINER imarom@cisco.com

LABEL RUN docker run --privileged --cap-add=ALL -v /mnt/huge:/mnt/huge -v /lib/modules:/lib/modules:ro -v /sys/bus/pci/devices:/sys/bus/pci/devices -v /sys/devices/system/node:/sys/devices/system/node -v /dev:/dev --name NAME -e NAME=NAME -e IMAGE=IMAGE IMAGE

COPY trex_cfg.yaml /etc/trex_cfg.yaml

RUN apt-get update
RUN apt-get install -y sudo gcc g++ python git zlib1g-dev pciutils vim kmod strace wget
RUN mkdir /scratch

WORKDIR /scratch
RUN git clone https://github.com/cisco-system-traffic-generator/trex-core.git 

WORKDIR /scratch/trex-core

CMD ["/bin/bash"]

