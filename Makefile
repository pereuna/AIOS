CC ?= cc
OBJCOPY ?= objcopy
PYTHON ?= python3
CURL ?= curl
CONTEXT ?= 1024
TOKENS ?= 128

MODEL_REV = 31b70e2e869a7173562077fd711b654946d38674
MODEL_BASE_URL ?= https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct/resolve/$(MODEL_REV)
MODEL_SOURCE_DIR ?= .model-source/SmolLM2-1.7B-Instruct
MODEL_SHA256 = a309d378bc03490e65f8e88a76ee2482f85751a248731ec3663b1647447b7620
MODEL_SOURCE_SHA256 = f55217be716b6a997b97b9d8d7eb6fad02e00858f5010ec24f64603c3a98a0e8
CONFIG_SHA256 = 994f50b16abb4ae00880baefe03c10260b5bd608d2bf586f7056ca05a534feea
TOKENIZER_SHA256 = 9ca9acddb6525a194ec8ac7a87f24fbba7232a9a15ffa1af0c1224fcd888e47c

GNU_EFI_URL ?= https://snapshot.debian.org/archive/debian/20260127T023013Z/pool/main/g/gnu-efi/gnu-efi_3.0.18-1%2Bdeb13u1_amd64.deb
GNU_EFI_SHA256 = 9248127ab870fbfe2da16ecd7c18f91cbd54795bba16f04808626b0e0be3a05f
GNU_EFI_DIR = .tools/gnu-efi
GNU_EFI_ROOT = $(CURDIR)/$(GNU_EFI_DIR)/usr
GNU_EFI_STAMP = $(GNU_EFI_DIR)/.installed
SYSTEM_EFI_READY := $(and $(wildcard /usr/include/efi/efi.h),\
	$(wildcard /usr/include/efi/x86_64/efibind.h),\
	$(wildcard /usr/lib/crt0-efi-x86_64.o),\
	$(wildcard /usr/lib/elf_x86_64_efi.lds),\
	$(wildcard /usr/lib/libefi.a),$(wildcard /usr/lib/libgnuefi.a))

EFI_ROOT ?= $(if $(SYSTEM_EFI_READY),/usr,$(GNU_EFI_ROOT))
EFI_BOOTSTRAP = $(if $(filter $(GNU_EFI_ROOT),$(EFI_ROOT)),$(GNU_EFI_STAMP))
EFI_CFLAGS = -O3 -std=c11 -Wall -Wextra -Wpedantic -ffreestanding -fno-builtin \
	-fno-stack-protector -fpic -fshort-wchar -fno-asynchronous-unwind-tables \
	-fno-unwind-tables -m64 -mno-red-zone -mno-avx -msse2 -mfpmath=sse \
	-mno-80387 -mno-mmx -ffp-contract=off -maccumulate-outgoing-args -DGNU_EFI_USE_MS_ABI \
	-I$(EFI_ROOT)/include/efi -I$(EFI_ROOT)/include/efi/x86_64
OBJECTS = .build/main.o .build/platform.o .build/mp.o .build/cpu.o .build/lib.o .build/math.o .build/fp.o
BOOT = dist/EFI/BOOT/BOOTX64.EFI

.PHONY: all clean model model-check test bench FORCE
all: $(BOOT) dist/model.bin

model: model.bin

model-check:
	@$(PYTHON) -c 'from tools.export_model import unicode_ranges; unicode_ranges()'

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

model.bin: tools/export_model.py tools/unicode-15.1.0.txt Makefile $(MODEL_SOURCE_DIR)/config.json \
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

$(GNU_EFI_STAMP):
	@command -v ar >/dev/null || { echo "GNU-EFI bootstrap requires ar" >&2; exit 1; }
	@command -v tar >/dev/null || { echo "GNU-EFI bootstrap requires tar" >&2; exit 1; }
	@mkdir -p $(GNU_EFI_DIR)
	@set -eu; \
	archive="$(GNU_EFI_DIR)/gnu-efi.deb.part"; \
	trap 'rm -f "$$archive"' EXIT; \
	$(CURL) --fail --location --retry 3 --output "$$archive" "$(GNU_EFI_URL)"; \
	echo "$(GNU_EFI_SHA256)  $$archive" | sha256sum --check -; \
	ar p "$$archive" data.tar.xz | tar -xJf - -C $(GNU_EFI_DIR); \
	test -f $(GNU_EFI_ROOT)/include/efi/efi.h; \
	test -f $(GNU_EFI_ROOT)/lib/libefi.a; \
	test -f $(GNU_EFI_ROOT)/lib/libgnuefi.a; \
	rm -f "$$archive"; \
	touch "$@"; \
	trap - EXIT

.build:
	mkdir -p $@

.build/%.o: baremetal/%.c baremetal/runtime.h Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -c $< -o $@

.build/main.o: neural.c .build/config.h
.build/mp.o .build/platform.o: baremetal/mp.h
.build/cpu.o: baremetal/cpu.h

.build/fp.o: baremetal/fp.S | .build
	$(CC) -m64 -c $< -o $@

.build/config.h: model.bin baremetal/image.py Makefile FORCE | .build
	$(PYTHON) baremetal/image.py --context $(CONTEXT) --tokens $(TOKENS) \
		--sha256 $(MODEL_SHA256) --output $@

.build/kernel.so: $(OBJECTS) $(EFI_BOOTSTRAP)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(OBJECTS) -L$(EFI_ROOT)/lib -lgnuefi -o $@

$(BOOT): .build/kernel.so
	mkdir -p $(@D)
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc -O pei-x86-64 --subsystem=10 $< $@.tmp
	mv $@.tmp $@
	sha256sum $@

dist/model.bin: model.bin
	mkdir -p $(@D)
	cp $< $@.tmp
	mv $@.tmp $@

MP_TEST_SOURCES = tests/mp_firmware.c baremetal/mp.c baremetal/cpu.c baremetal/fp.S
MP_TEST_DEPS = $(MP_TEST_SOURCES) tests/mp_firmware.h baremetal/mp.h baremetal/cpu.h baremetal/runtime.h

.build/test-inference: tests/inference.c neural.c baremetal/math.c $(MP_TEST_DEPS) Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -pthread tests/inference.c baremetal/math.c $(MP_TEST_SOURCES) -o $@

.build/test-parallel: tests/parallel.c $(MP_TEST_DEPS) Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -pthread tests/parallel.c $(MP_TEST_SOURCES) -o $@

.build/test-simd: tests/simd.c neural.c baremetal/math.c $(MP_TEST_DEPS) Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -Wno-unused-function -pthread tests/simd.c baremetal/math.c $(MP_TEST_SOURCES) \
		-Wl,--wrap=bm_avx2_begin -o $@

.build/test-file-loader: tests/file_loader.c baremetal/platform.c baremetal/mp.h baremetal/runtime.h Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) tests/file_loader.c -o $@

test: all .build/test-inference .build/test-file-loader .build/test-parallel .build/test-simd
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'
	.build/test-file-loader
	.build/test-parallel
	.build/test-simd
	$(PYTHON) tests/verify.py

bench: model.bin .build/test-inference
	$(PYTHON) tests/bench.py

clean:
	rm -rf .build dist
