ARCH = loongarch64
TOOLPREFIX = loongarch64-unknown-linux-gnu-
CC = $(TOOLPREFIX)gcc
CXX = $(TOOLPREFIX)g++
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

export ARCH TOOLPREFIX CC CXX AS LD OBJCOPY OBJDUMP

.PHONY: all kernel bootloader install efilib

all: kernel bootloader

efilib:
	prefix=$(TOOLPREFIX)  make -C gnu-efi

kernel:
	make -C ddLoongKernel

bootloader: efilib
	make -C simpleBootloader

install: kernel bootloader
	[ -f a.img ] || (qemu-img create a.img 10M && mkfs.fat a.img) ;\
	mount a.img /mnt ;\
	cp simpleBootloader/hello.efi /mnt ;\
	cp ddLoongKernel/kernel /mnt ;\
	umount -R /mnt

qemu: install
	qemu-system-loongarch64 -m 1G \
	--cpu la464 \
	--machine virt \
	-usb \
	-device usb-ehci,id=ehci \
	-bios /usr/share/qemu/qemu-loongarch.fd \
	-drive id=disk,file=a.img,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-vga std \
	-device usb-kbd

clean:
	make -C ddLoongKernel clean
	make -C simpleBootloader clean
	make -C gnu-efi clean
	if [ -f a.img ]; then rm a.img; fi
