# 文件: Makefile
obj-m += slab_stat.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod slab_stat.ko

uninstall:
	sudo rmmod slab_stat

info:
	modinfo slab_stat.ko

.PHONY: all clean install uninstall info
