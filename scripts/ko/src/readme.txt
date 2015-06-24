

1. make sure you have install this

Fedora
  $sudo yum install kernel-headers
or
Ubunto   
  $sudo apt-get install linux-headers-$(uname -r)

2. from this dir do this

$make 
$sudo modprobe uio
$sudo insmod igb_uio.ko

