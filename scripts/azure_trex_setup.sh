#!/bin/bash

sudo modprobe uio_hv_generic

NET_UUID="f8615163-df3e-46c5-913f-f2d2f965ed0e"
echo $NET_UUID | sudo tee /sys/bus/vmbus/drivers/uio_hv_generic/new_id

DEV_UUID=$(basename $(readlink /sys/class/net/eth1/device))
echo $DEV_UUID | sudo tee /sys/bus/vmbus/drivers/hv_netvsc/unbind
echo $DEV_UUID | sudo tee /sys/bus/vmbus/drivers/uio_hv_generic/bind

DEV_UUID=$(basename $(readlink /sys/class/net/eth2/device))
echo $DEV_UUID | sudo tee /sys/bus/vmbus/drivers/hv_netvsc/unbind
echo $DEV_UUID | sudo tee /sys/bus/vmbus/drivers/uio_hv_generic/bind


