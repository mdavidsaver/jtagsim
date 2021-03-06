ifneq ($(KERNELRELEASE),)
include Kbuild
else

KERNELDIR := /lib/modules/`uname -r`/build
modules clean::
	$(MAKE) -C $(KERNELDIR) M=`pwd` $@

CPPFLAGS_kernsim = -DPPDEV

sim: benchsim kernsim

benchsim: jtag.c

benchsim kernsim: benchtest.c
	gcc -o $@ -g -Wall $(CPPFLAGS_$@) $<

clean::
	rm -f benchsim kernsim

endif
