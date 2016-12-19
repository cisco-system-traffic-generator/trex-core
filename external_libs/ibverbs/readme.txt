sudo yum install libnl3-devel

download MLNX_OFED_SRC-3.4-1.0.0.0.tgz


open [libibverbs-1.2.1mlnx1-OFED.3.4.0.1.4.34100.src.rpm,
     libmlx5-1.2.1mlnx1,]
     

how to open 
-----------------
   rpm2cpio myrpmfile.rpm | cpio -idmv
   tar -xvzf myrpmfile.rpm
   ./autogen.sh
   ./configure
   make

copy headers
-----------------
   copy lib  src/.libs to  .
   copy header to 

for mlx5 driver 
-----------------
   for mlx5 
   sudo make install
   then copy the /usr/local/include/infiniband/mlx5_hw.h to header folder 
   


