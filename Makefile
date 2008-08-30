
ifneq ($(KERNELRELEASE),)
obj-m := radeonhdfb.o
radeonhdfb-objs := radeonhdfb_base.o
else
KDIR := $(shell ./findkernel)
PWD := $(shell pwd)

default: radeonhdfb.ko

radeonhdfb.ko: radeonhdfb_base.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f .*.cmd *.o *.ko *.s Module.symvers

VERSION := 0.1

endif

