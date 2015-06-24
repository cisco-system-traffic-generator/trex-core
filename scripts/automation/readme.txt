README - trex_perf.py
=====================

This script uses the T-Rex RESTfull client-server conrtol plane achitecture and tries to find the maximum M (platform factor) for trex before hitting one of two stopping conditions:
(*) Packet drops
(*) High latency.
    Since high latency can change from one platform to another, and might suffer from kickoff peak (espicially at VM), it is the user responsibility to provide the latency condition. 
    A common value used by non-vm machines is 1000, where in VM machines values around 5000 are more common.

please note that '-f' and '-c' options are mandatory. 

Also, this is the user's responsibility to make sure a T-Rex is running, listening to relevant client request coming from this script.

example for finding max M (between 10 to 100) with imix_fast_1g.yaml traffic profile:
./trex_perf.py -m 10 100 -c config/trex-hhaim.cfg all drop -f cap2/imix_fast_1g.yaml
