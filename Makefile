.PHONY: all kernel bootloader install

all: kernel bootloader

kernel:
	make -C ddLoongKernel

bootloader:
	make -C simpleBootloader

install: kernel bootloader
	[ -f a.img ] || (qemu-img create a.img 10M && mkfs.fat a.img) ;\
	mount a.img /mnt ;\
	cp bootloader/hello.efi /mnt ;\
	cp kernel/kernel /mnt ;\
	umount -R /mnt

qemu: install
	./qemu-system-loongarch64 -m 1G \
	--cpu la464 \
	--machine virt \
	-usb \
	-device usb-ehci,id=ehci \
	-bios ./QEMU_EFI.fd \
	-drive id=disk,file=a.img,if=none \
	-device ahci,id=ahci \
	-device ide-hd,drive=disk,bus=ahci.0 \
	-vga std \
	-device usb-kbd \
	-monitor stdio

clean:
	cd kernel && make clean && cd .. &&\
	cd bootloader && make clean
