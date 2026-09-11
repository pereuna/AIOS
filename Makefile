CC ?= cc
PYTHON ?= python3
CURL ?= curl
CONTEXT ?= 1024
TOKENS ?= 128

MODEL_URL ?= https://github.com/pereuna/AIOS/releases/download/smollm2-135m-q4-v1/model.bin
MODEL_SHA256 = 9f8b9b089b6a0fba150438567607e6969cc1ffe8e4e8d01895ecb6d067675a74

EFI_ROOT ?= $(if $(wildcard /usr/include/efi/efi.h),/usr,$(CURDIR)/.tools/gnu-efi/usr)
EFI_CFLAGS = -O3 -std=c11 -Wall -Wextra -Wpedantic -ffreestanding -fno-builtin \
	-fno-stack-protector -fpic -fshort-wchar -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -m64 -mno-red-zone -mno-avx -msse2 -mfpmath=sse \
	-mno-80387 -mno-mmx -maccumulate-outgoing-args -DGNU_EFI_USE_MS_ABI \
	-I$(EFI_ROOT)/include/efi -I$(EFI_ROOT)/include/efi/x86_64
OBJECTS = .build/main.o .build/platform.o .build/lib.o .build/math.o \
	.build/fp.o .build/payload.o
BOOT = dist/EFI/BOOT/BOOTX64.EFI

.PHONY: all clean model FORCE
all: $(BOOT)

model: model.bin

model.bin:
	@set -eu; \
	part="$@.part"; \
	trap 'rm -f "$$part"' EXIT; \
	$(CURL) --fail --location --retry 3 --output "$$part" "$(MODEL_URL)"; \
	echo "$(MODEL_SHA256)  $$part" | sha256sum --check -; \
	mv "$$part" "$@"; \
	trap - EXIT

.build:
	mkdir -p $@

.build/%.o: baremetal/%.c baremetal/runtime.h Makefile | .build
	$(CC) $(EFI_CFLAGS) -c $< -o $@

.build/main.o: neural.c

.build/fp.o: baremetal/fp.S | .build
	$(CC) -m64 -c $< -o $@

.build/payload.bin: model.bin baremetal/image.py Makefile FORCE | .build
	$(PYTHON) baremetal/image.py --context $(CONTEXT) --tokens $(TOKENS) --output $@

.build/payload.o: baremetal/payload.S .build/payload.bin
	$(CC) -m64 -c $< -o $@

.build/kernel.so: $(OBJECTS)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(OBJECTS) -L$(EFI_ROOT)/lib -lgnuefi -o $@

$(BOOT): .build/kernel.so
	mkdir -p $(@D)
	objcopy -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc --target=efi-app-x86_64 $< $@.tmp
	mv $@.tmp $@
	sha256sum $@

clean:
	rm -rf .build dist
