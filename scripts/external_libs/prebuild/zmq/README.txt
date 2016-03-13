How to build PyZMQ for your system
==================================

I'll desrcibe a harder task - how to build cross platform PyZMQ.
Building for current system will be much easier.

for the example I will build 32 bit Python 3 PyZMQ on a 64 bit machine.

1. build ZMQ library
    a. Unzip zeromq-4.0.2.tar.gz
    b. create a dir called zermoq-4.0.2-bin
    c  set CC, CXX, CXXLD, CFLAGS, CXXFLAGS and LDSHARED correctly if you need another toolchain
    d. configure: ./configure --prefix=zeromq-4.0.2-bin
    e. make -j64
    f. make install

now we have ZMQ binaries built to zeromq-4.0.2-bin


2. build PyZMQ

    a. unzip pyzmq-14.5.0
    b. create a dir called pyzmq-14.5.0-bin
    c. setenv LD_LIBRARY_PATH zeromq-4.0.2-bin/lib
    c. *FOR CROSS COMPILE* we need a setup.cfg file skip to D
        for non cross compile - you can simply run:
	python setup.py configure --zmq=zeromq-4.0.2-bin
	python setup.py build --prefix=pyzmq-14.5.0-bin


    d. *CROSS COMPILE*
       create a setup.cfg accroding to setup.cfg.template
       with include_libs, and directory_libs
       make sure they include the Python include/lib directories
       as well as ZMQ
       for example, for Python 3 i had to download the Python 3 development package


The final output:
prebuild/zmq/pyzmq-14.5.0-bin/lib/python3.4/site-packages/zmq

