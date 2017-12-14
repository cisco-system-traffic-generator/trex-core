base version : dpdk17.11

1) some XL710 INTR patch created latency issue. not clear why (reverted the patch)

---------------------------
* How to move to new DPDK 
---------------------------

1)# pul latest with tags 
git pull -v origin master --tags
 
2) # checkout the old branch 
git checkout -b 17_02 v18.02

3)
# checkout the new branch 
git checkout -b 17_11 v17.11

4)
#copy lib and driver folders 

5) make config file 
+ and compare the changes from old version 

$make config T=x86_64-native-linuxapp-gcc
ll build/include/rte_config.h 

6) check which files where change location update waf makefile and path 

7) build vanila DPDK make sure it compile 
 ./b build --target=dpdk-64
 
8) now add our patch and recompile 

 ./b build --target=dpdk-64


10) try to complie all the software and add the files 

---------------------------




