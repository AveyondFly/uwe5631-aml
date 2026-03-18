# Cross compile settings for ROCKNIX
TOOLCHAIN_PATH ?= /home/ubuntu/distribution/build.ROCKNIX-RK3326.aarch64/toolchain/bin
KERNEL_SRC ?= /home/ubuntu/distribution/build.ROCKNIX-S905L3A.aarch64/build/linux-6.18.13
ARCH ?= arm64
CROSS_COMPILE ?= $(TOOLCHAIN_PATH)/aarch64-rocknix-linux-gnueabi-

PWD := $(shell pwd)

all: modules

modules:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(PWD)/BSP CFG_AML_WIFI_DEVICE_UWE5621=y modules
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(PWD)/WIFI TARGET_BUILD_VARIANT=user modules
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNEL_SRC) M=$(PWD)/BT/tty-sdio CURFOLDER=$(PWD)/BSP modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/BSP clean
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/WIFI clean
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD)/BT/tty-sdio clean

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 M=$(PWD)/BSP -C $(KERNEL_SRC) modules_install
	$(MAKE) INSTALL_MOD_STRIP=1 M=$(PWD)/WIFI -C $(KERNEL_SRC) modules_install
	$(MAKE) INSTALL_MOD_STRIP=1 M=$(PWD)/BT/tty-sdio -C $(KERNEL_SRC) modules_install

