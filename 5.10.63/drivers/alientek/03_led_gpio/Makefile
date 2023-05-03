CURRENT_PATH := $(shell pwd)
KERNELDIR := $(HOME)/workspace/02.source/linux.dev/

obj-m := gpio_led.o

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
