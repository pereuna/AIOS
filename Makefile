CC ?= cc
PYTHON ?= python3
CURL ?= curl
CONTEXT ?= 1024
TOKENS ?= 128

MODEL_REV = 12fd25f77366fa6b3b4b768ec3050bf629380bac
MODEL_BASE_URL ?= https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct/resolve/$(MODEL_REV)
MODEL_SOURCE_DIR ?= .model-source
MODEL_SHA256 = 9f8b9b089b6a0fba150438567607e6969cc1ffe8e4e8d01895ecb6d067675a74
MODEL_SOURCE_SHA256 = 5af571cbf074e6d21a03528d2330792e532ca608f24ac70a143f6b369968ab8c
CONFIG_SHA256 = 8eb740e8bbe4cff95ea7b4588d17a2432deb16e8075bc5828ff7ba9be94d982a
TOKENIZER_SHA256 = 9ca9acddb6525a194ec8ac7a87f24fbba7232a9a15ffa1af0c1224fcd888e47c

EFI_ROOT ?= $(if $(wildcard /usr/include/efi/efi.h),/usr,$(CURDIR)/.tools/gnu-efi/usr)
EFI_CFLAGS = -O3 -std=c11 -Wall -Wextra -Wpedantic -ffreestanding -fno-builtin \
	-fno-stack-protector -fpic -fshort-wchar -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -m64 -mno-red-zone -mno-avx -msse2 -mfpmath=sse \
	-mno-80387 -mno-mmx -maccumulate-outgoing-args -DGNU_EFI_USE_MS_ABI \
	-I$(EFI_ROOT)/include/efi -I$(EFI_ROOT)/include/efi/x86_64
OBJECTS = .build/main.o .build/platform.o .build/lib.o .build/math.o \
	.build/fp.o .build/payload.o
BOOT = dist/EFI/BOOT/BOOTX64.EFI

.PHONY: all clean model model-check FORCE
all: $(BOOT)

model: model.bin

model-check:
	@$(PYTHON) -c 'import numpy, unicodedata; assert unicodedata.unidata_version == "15.1.0", "Python Unicode data must be version 15.1.0"'

$(MODEL_SOURCE_DIR):
	mkdir -p $@

$(MODEL_SOURCE_DIR)/config.json: | model-check $(MODEL_SOURCE_DIR)
	@set -eu; part="$@.part"; trap 'rm -f "$$part"' EXIT; \
	$(CURL) --fail --location --retry 3 --output "$$part" "$(MODEL_BASE_URL)/config.json"; \
	echo "$(CONFIG_SHA256)  $$part" | sha256sum --check -; \
	mv "$$part" "$@"; trap - EXIT

$(MODEL_SOURCE_DIR)/tokenizer.json: | model-check $(MODEL_SOURCE_DIR)
	@set -eu; part="$@.part"; trap 'rm -f "$$part"' EXIT; \
	$(CURL) --fail --location --retry 3 --output "$$part" "$(MODEL_BASE_URL)/tokenizer.json"; \
	echo "$(TOKENIZER_SHA256)  $$part" | sha256sum --check -; \
	mv "$$part" "$@"; trap - EXIT

$(MODEL_SOURCE_DIR)/model.safetensors: | model-check $(MODEL_SOURCE_DIR)
	@set -eu; part="$@.part"; trap 'rm -f "$$part"' EXIT; \
	$(CURL) --fail --location --retry 3 --output "$$part" "$(MODEL_BASE_URL)/model.safetensors"; \
	echo "$(MODEL_SOURCE_SHA256)  $$part" | sha256sum --check -; \
	mv "$$part" "$@"; trap - EXIT

model.bin: tools/export_model.py $(MODEL_SOURCE_DIR)/config.json \
	$(MODEL_SOURCE_DIR)/tokenizer.json $(MODEL_SOURCE_DIR)/model.safetensors
	@set -eu; \
	part="$@.part"; \
	trap 'rm -f "$$part"' EXIT; \
	$(PYTHON) tools/export_model.py --config $(MODEL_SOURCE_DIR)/config.json \
		--tokenizer $(MODEL_SOURCE_DIR)/tokenizer.json \
		--weights $(MODEL_SOURCE_DIR)/model.safetensors --output "$$part"; \
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
