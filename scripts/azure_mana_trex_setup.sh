#!/bin/bash

ip link

PRIMARY1="eth1"
SECONDARY1="`ip -br link show master $PRIMARY1 | awk '{ print $1 }'`"
# Get mac address for MANA interface (should match primary)
MANA_MAC1="`ip -br link show master $PRIMARY1 | awk '{ print $3 }'`"
# get MANA device bus info to pass to TREX/DPDK
BUS_INFO1="`ethtool -i $SECONDARY1 | grep bus-info | awk '{ print $2 }'`"

sudo ethtool -K $PRIMARY1 gso off gro off tso off lro off
sudo ethtool -K $SECONDARY1 gso off gro off tso off
sleep 2

# Set MANA inetrfaces DOWN bore starting TREX/DPDK
sudo ip link set $PRIMARY1 down
sudo ip link set $SECONDARY1 down
sleep 2

PRIMARY2="eth2"
SECONDARY2="`ip -br link show master $PRIMARY2 | awk '{ print $1 }'`"
# Get mac address for MANA interface (should match primary)
MANA_MAC2="`ip -br link show master  $PRIMARY2 | awk '{ print $3 }'`"
# get MANA device bus info to pass to TREX/DPDK
BUS_INFO2="`ethtool -i $SECONDARY2 | grep bus-info | awk '{ print $2 }'`"

sudo ethtool -K $PRIMARY2 gso off gro off tso off lro off
sudo ethtool -K $SECONDARY2 gso off gro off tso off
sleep 2

# Set MANA insterfaces DOWN before starting TREX/DPDK
sudo ip link set $PRIMARY2 down
sudo ip link set $SECONDARY2 down
sleep 2

sudo modprobe uio_hv_generic
sleep 2

NET_UUID="f8615163-df3e-46c5-913f-f2d2f965ed0e"
echo $NET_UUID | sudo tee /sys/bus/vmbus/drivers/uio_hv_generic/new_id

echo " Unbind $PRIMARY1 from kernel"
DEV_UUID1=$(basename $(readlink /sys/class/net/$PRIMARY1/device))
echo $DEV_UUID1 | sudo tee /sys/bus/vmbus/drivers/hv_netvsc/unbind
echo " Bind $PRIMARY1 to UIO"
echo $DEV_UUID1 | sudo tee /sys/bus/vmbus/drivers/uio_hv_generic/bind

echo " Unbind $PRIMARY2 from kernel"
DEV_UUID2=$(basename $(readlink /sys/class/net/$PRIMARY2/device))
echo $DEV_UUID2 | sudo tee /sys/bus/vmbus/drivers/hv_netvsc/unbind
echo " Bind $PRIMARY2 to UIO"
echo $DEV_UUID2 | sudo tee /sys/bus/vmbus/drivers/uio_hv_generic/bind

ip link

echo ""
echo "Info for updating trex_cfg.yaml: "
echo "  interfaces: ['$BUS_INFO1', '$BUS_INFO2'] "
echo "  ext_dpdk_opt: ['--vdev=net_vdev_netvsc,ignore=1', '--vdev=net_vdev_netvsc,ignore=1'] "
echo "  interfaces_vdevs : ['$DEV_UUID1', '$DEV_UUID2'] "
echo ""
