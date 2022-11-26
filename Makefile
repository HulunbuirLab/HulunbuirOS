ARCH = loongarch64
ABI = lp64d
TOOLPREFIX = loongarch64-linux-gnu-
CC = $(TOOLPREFIX)gcc
CXX = $(TOOLPREFIX)g++
AS = $(TOOLPREFIX)as
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

export ARCH TOOLPREFIX CC CXX AS LD OBJCOPY OBJDUMP ABI

.PHONY: all kernel bootloader install efilib

all: kernel bootloader

efilib:
	prefix=$(TOOLPREFIX)  make -C gnu-efi

kernel:
	make -C ddLoongKernel

bootloader:
	make -C simpleBootloader

install: kernel bootloader
	[ -f a.img ] || (qemu-img create a.img 10M && mkfs.fat a.img) ;\
	sudo mount a.img /mnt ;\
	sudo cp simpleBootloader/hello.efi /mnt ;\
	sudo cp ddLoongKernel/kernel /mnt ;\
	sudo umount -R /mnt
	sudo chown $(USER) a.img

qemu: install
	qemu-system-loongarch64 -m 1G \
	--cpu la464 \
	--machine virt \
	-usb \
	-bios /usr/share/edk2-ovmf/loongarch64/OVMF_CODE.fd \
	-device usb-ehci,id=ehci \
	-drive id=disk,file=a.img,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-vga std \
	-device usb-kbd

clean:
	make -C ddLoongKernel clean
	make -C simpleBootloader clean
	if [ -f a.img ]; then rm a.img; fi
