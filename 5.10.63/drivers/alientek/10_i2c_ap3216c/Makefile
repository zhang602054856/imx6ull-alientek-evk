CURRENT_PATH := $(shell pwd)
KERNELDIR := $(HOME)/workspace/02.source/linux/linux-5.10.63/

obj-m := misc_ap32xx.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
