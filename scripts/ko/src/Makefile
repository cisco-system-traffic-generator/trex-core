# obj-m is a list of what kernel modules to build.  The .o and other
# objects will be automatically built from the corresponding .c file -
# no need to list the source files explicitly.

obj-m := igb_uio.o 

# KDIR is the location of the kernel source.  The current standard is
# to link to the associated source tree from the directory containing
# the compiled modules.
KDIR  := /lib/modules/$(shell uname -r)/build

# PWD is the current working directory and the location of our module
# source files.
PWD   := $(shell pwd)

# default is the default make target.  The rule here says to run make
# with a working directory of the directory containing the kernel
# source and compile only the modules in the PWD (local) directory.
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	-rm *.o >> test.log
	-rm *.ko >> test.log
	-rm *.*.cmd >> test.log
	-rm *.mod.c >> test.log
	-rm modules.order >> test.log
	-rm Module.symvers >> test.log
	-rm -rf .tmp_versions >> test.log
	rm test.log
	
	
     
# install the new driver 
install:
	mkdir -p ../`uname -r`/
	cp igb_uio.ko ../`uname -r`/	
	
