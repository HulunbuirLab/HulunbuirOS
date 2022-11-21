OBJS            = fileops.o elf_parse.o loader.o
TARGET          = hello.efi
EFIINC          = ../gnu-efi/inc
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
EFILIB             = ../gnu-efi/$(ARCH)/gnuefi
LIB		= ../gnu-efi/$(ARCH)/lib
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = ../gnu-efi/gnuefi/elf_$(ARCH)_efi.lds
CFLAGS          = $(EFIINCS) -I../ -fno-stack-protector -fpic \
                  -fshort-wchar -Wall

CFLAGS += -march=loongarch64 -mabi=lp64d -g -O2\
	  -fno-strict-aliasing \
          -ffreestanding -fno-stack-check \
          $(if $(findstring gcc,$(CC)),-fno-merge-all-constants,)

LDFLAGS         = -nostdlib --no-undefined -shared \
                  --build-id=sha1 -Bsymbolic --defsym=EFI_SUBSYSTEM=0xa -T $(EFI_LDS) -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS)

all: $(TARGET)

%.so: $(OBJS)
	@$(LD) $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

%.efi: %.so
	@$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
            -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
            -j .reloc -O binary $^ $@

clean:
	@rm -f *.o *.efi *.so

