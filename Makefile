ifneq ($(KERNELRELEASE),)
include Kbuild
else

KERNELDIR := /lib/modules/`uname -r`/build
modules clean::
	$(MAKE) -C $(KERNELDIR) M=`pwd` $@

endif
