obj-m += nifs.o
nifs-objs := source/nifs.o source/nifs_utils.o

PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build
EXTRA_CFLAGS := -Wall -g

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf .cache

.PHONY: all clean