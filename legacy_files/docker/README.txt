how to use TRex under docker ?

1. install docker:
    wget -qO- https://get.docker.com/ | sh

2. go to the desired path. e.g. ubuntu/16.10/

3. docker build -t trex-dev-ubuntu:16.10 .

4. docker run -it --privileged --cap-add=ALL -v /mnt/huge:/mnt/huge -v /sys/bus/pci/devices:/sys/bus/pci/devices -v /sys/devices/system/node:/sys/devices/system/node -v /dev:/dev trex-dev-ubuntu:16.10
