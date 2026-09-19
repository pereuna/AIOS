CC ?= cc
OBJCOPY ?= objcopy
PYTHON ?= python3
CURL ?= curl
CONTEXT ?= 2048
TOKENS ?= 512
.DEFAULT_GOAL := all

MODEL_REV = 2e1fd397ee46e1388853d2af2c993145b0f1098a
MODEL_BASE_URL ?= https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct/resolve/$(MODEL_REV)
MODEL_SOURCE_DIR ?= .model-source/Qwen2.5-Coder-1.5B-Instruct
MODEL_SHA256 = $(shell $(PYTHON) -c 'import json; print(json.load(open("model.json"))["sha256"])')
MODEL_FILES = config.json tokenizer.json tokenizer_config.json model.safetensors LICENSE
MODEL_SOURCES = $(addprefix $(MODEL_SOURCE_DIR)/,$(MODEL_FILES))

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
OBJECTS = .build/main.o .build/platform.o .build/process.o .build/process-arch.o .build/process_console.o .build/calc.o .build/ir.o .build/mp.o .build/cpu.o .build/lib.o .build/math.o .build/fp.o
BOOT = dist/EFI/BOOT/BOOTX64.EFI

.PHONY: all clean model model-check test bench win FORCE

# Native console has no EFI, model-download, Torch or QEMU dependency.
CONSOLE_ARGS ?=
CONSOLE_MODEL ?= $(if $(wildcard .build/console-model.bin),.build/console-model.bin,model.bin)
HOST_CFLAGS = -O3 -std=c11 -Wall -Wextra -Wpedantic -m64 -mno-avx -msse2 \
	-mfpmath=sse -mno-80387 -mno-mmx -ffp-contract=off -fno-builtin
CONSOLE_SOURCES = linux/model.c linux/runtime.c baremetal/math.c baremetal/cpu.c baremetal/fp.S
CONSOLE_DEPS = baremetal/ir.h baremetal/calc.h baremetal/calc_model.h neural.c baremetal/runtime.h baremetal/cpu.h baremetal/calc_prompt.h \
	baremetal/model_tokens.h baremetal/nfc.h baremetal/nfc_data.h

.PHONY: console console-build console-test console-model
console-build: .build/aios-model

.build/aios-model: $(CONSOLE_SOURCES) $(CONSOLE_DEPS) Makefile | .build
	$(CC) $(HOST_CFLAGS) -pthread $(CONSOLE_SOURCES) -o $@

console: console-build
	$(PYTHON) linux/console.py --model "$(CONSOLE_MODEL)" $(CONSOLE_ARGS)

console-test: console-build .build/test-linux-runtime
	.build/test-linux-runtime
	$(PYTHON) -m unittest discover -s tests -p 'test_linux_console.py'

.build/test-linux-runtime: tests/linux_runtime.c linux/runtime.c baremetal/fp.S baremetal/runtime.h Makefile | .build
	$(CC) $(HOST_CFLAGS) -pthread tests/linux_runtime.c linux/runtime.c baremetal/fp.S -o $@

# Convert an existing local checkpoint only; never fetch source files here.
# Separate output preserves model.bin, including older or custom checkpoints.
console-model: | .build
	@set -eu; part=".build/console-model.bin.part"; trap 'rm -f "$$part"' EXIT; \
	$(PYTHON) tools/export_model.py --config "$(MODEL_SOURCE_DIR)/config.json" \
		--tokenizer "$(MODEL_SOURCE_DIR)/tokenizer.json" \
		--weights "$(MODEL_SOURCE_DIR)/model.safetensors" --output "$$part"; \
	mv "$$part" .build/console-model.bin; trap - EXIT

WIN_DEST ?= /mnt/c/temp
WIN_TRAINING_FILES = training/*.py training/*.md training/requirements*.txt
WIN_TOOL_FILES = tools/export_model.py tools/split_model.py tools/unicode-15.1.0.txt

win:
	@set -eu; \
	test -d "$(WIN_DEST)" || { echo "Windows destination does not exist: $(WIN_DEST)" >&2; exit 1; }; \
	test -w "$(WIN_DEST)" || { echo "Windows destination is not writable from this WSL: $(WIN_DEST)" >&2; exit 1; }; \
	mkdir -p "$(WIN_DEST)/training/data" "$(WIN_DEST)/tools" "$(WIN_DEST)/baremetal"; \
	cp -p $(WIN_TRAINING_FILES) "$(WIN_DEST)/training/"; \
	cp -p $(WIN_TOOL_FILES) "$(WIN_DEST)/tools/"; \
	cp -p training/AIOS_TRAINING_WINDOWS.py "$(WIN_DEST)/AIOS_TRAINING_WINDOWS.py"; \
	echo "Windows training bundle copied to $(WIN_DEST)"; \
	printf '%s\n' 'Run in PowerShell: python C:\temp\AIOS_TRAINING_WINDOWS.py check'
MODEL_PARTS = dist/model.000
all: $(BOOT) $(MODEL_PARTS)

QEMU ?= qemu-system-x86_64
QEMU_ACCEL ?= tcg
QEMU_CPU ?= max
OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS ?= /usr/share/OVMF/OVMF_VARS_4M.fd
QEMU_DISPLAY ?= gtk

.build/qemu-interactive/aios.img: $(BOOT) $(MODEL_PARTS)
	mkdir -p $(@D)
	truncate -s 0 $@
	truncate -s 2G $@
	mformat -i $@ -F ::
	mcopy -i $@ -s dist/EFI ::/
	mcopy -i $@ $(MODEL_PARTS) ::/

.PHONY: qemu
qemu: .build/qemu-interactive/aios.img
	@test -f .build/qemu-interactive/OVMF_VARS_4M.fd || cp "$(OVMF_VARS)" .build/qemu-interactive/OVMF_VARS_4M.fd
	$(QEMU) -machine q35 -accel $(QEMU_ACCEL) -cpu $(QEMU_CPU) -smp 4 -m 2048 \
		-drive "if=pflash,format=raw,readonly=on,file=$(OVMF_CODE)" \
		-drive if=pflash,format=raw,file=.build/qemu-interactive/OVMF_VARS_4M.fd \
		-drive format=raw,snapshot=on,file=.build/qemu-interactive/aios.img \
		-display $(QEMU_DISPLAY) -vga std -net none -no-reboot

model: model.bin

model-check:
	@$(PYTHON) -c 'from tools.export_model import unicode_ranges; unicode_ranges()'

$(MODEL_SOURCES): model.json | model-check
	$(PYTHON) tools/download_model.py --directory $(MODEL_SOURCE_DIR) --base-url $(MODEL_BASE_URL) --file $(@F)

model.bin: tools/export_model.py tools/unicode-15.1.0.txt model.json Makefile $(MODEL_SOURCES)
	@set -eu; part="$@.part"; trap 'rm -f "$$part"' EXIT; \
	$(PYTHON) tools/export_model.py --config $(MODEL_SOURCE_DIR)/config.json \
		--tokenizer $(MODEL_SOURCE_DIR)/tokenizer.json \
		--weights $(MODEL_SOURCE_DIR)/model.safetensors --output "$$part"; \
	echo "$(MODEL_SHA256)  $$part" | sha256sum --check -; \
	mv "$$part" "$@"; trap - EXIT

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

.build/process.o: baremetal/process.c baremetal/process.h baremetal/runtime.h Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -c $< -o $@

.build/process-arch.o: baremetal/process.S baremetal/process.h Makefile $(EFI_BOOTSTRAP) | .build
	$(CC) -m64 -c $< -o $@

.build/main.o: baremetal/nfc.h baremetal/nfc_data.h baremetal/model_tokens.h neural.c .build/config.h baremetal/process_console.h baremetal/calc.h baremetal/calc_model.h baremetal/calc_prompt.h
.build/calc.o: baremetal/calc.h baremetal/ir.h baremetal/process.h
.build/ir.o: baremetal/ir.h baremetal/process.h
.build/main.o .build/process-test-main.o .build/calc-live.o: baremetal/ir.h
.build/process_console.o: baremetal/process.h baremetal/process_console.h
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

$(MODEL_PARTS): model.bin tools/split_model.py
	$(PYTHON) tools/split_model.py model.bin dist

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

.build/test-process-console: tests/process_console.c baremetal/process_console.c baremetal/process_console.h baremetal/process.h | .build
	$(CC) -O2 -std=c11 -Wall -Wextra -Wpedantic tests/process_console.c baremetal/process_console.c -o $@

.build/test-inference .build/test-simd : baremetal/calc_prompt.h baremetal/nfc.h baremetal/nfc_data.h baremetal/model_tokens.h

test: all .build/test-ir .build/test-calc .build/test-calc-model .build/test-inference .build/test-file-loader .build/test-parallel .build/test-simd .build/test-process-console
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'
	.build/test-file-loader
	.build/test-parallel
	.build/test-simd
	.build/test-process-console
	.build/test-calc
	.build/test-calc-model
	.build/test-ir
	$(PYTHON) tests/verify.py

bench: model.bin .build/test-inference
	$(PYTHON) tests/bench.py

.build/process-test-main.o: tests/process_uefi.c tests/ir_cases.h baremetal/calc.h baremetal/process.h baremetal/process_console.h baremetal/runtime.h $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -c $< -o $@

.build/process-test.so: .build/process-test-main.o $(filter-out .build/main.o,$(OBJECTS)) $(EFI_BOOTSTRAP)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(filter %.o,$^) --wrap=bm_pages_alloc --wrap=bm_pages_free -L$(EFI_ROOT)/lib -lgnuefi -o $@

.build/process-test.efi: .build/process-test.so
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc -O pei-x86-64 --subsystem=10 $< $@

.PHONY: test-process
test-process: .build/process-test.efi
	$(PYTHON) tests/process_qemu.py
	$(PYTHON) tests/process_qemu.py --cpus 4

.build/model-test-main.o: tests/model_uefi.c neural.c baremetal/nfc.h baremetal/nfc_data.h baremetal/model_tokens.h baremetal/calc_prompt.h .build/config.h $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -Wno-unused-function -c $< -o $@

.build/model-test.so: .build/model-test-main.o $(filter-out .build/main.o,$(OBJECTS)) $(EFI_BOOTSTRAP)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(filter %.o,$^) -L$(EFI_ROOT)/lib -lgnuefi -o $@

.build/model-test.efi: .build/model-test.so
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc -O pei-x86-64 --subsystem=10 $< $@

.PHONY: test-model-uefi
test-model-uefi: .build/model-test.efi model.bin
	$(PYTHON) tests/process_qemu.py --image .build/model-test.efi --model model.bin \
		--memory 2048 --timeout 600 --accel kvm --cpus 4

clean:
	rm -rf .build dist

.build/test-calc: tests/calc.c baremetal/calc.c baremetal/ir.c baremetal/ir.h baremetal/calc.h baremetal/process.h | .build
	$(CC) -O2 -std=c11 -Wall -Wextra -Wpedantic tests/calc.c baremetal/calc.c baremetal/ir.c -o $@

.build/test-ir: tests/ir.c tests/ir_cases.h baremetal/ir.c baremetal/ir.h baremetal/calc.h baremetal/process.h | .build
	$(CC) -O2 -std=c11 -Wall -Wextra -Wpedantic tests/ir.c baremetal/ir.c -o $@

# Optional independent LLVM oracle; llvmlite is a HOST-ONLY test dependency.
LLVM_PYTHON ?= $(PYTHON)
.build/ir-llvm-cases.h: tests/verify_ir.py .build/test-ir
	$(LLVM_PYTHON) tests/verify_ir.py --output $@

.build/ir-llvm-main.o: tests/ir_llvm_uefi.c .build/ir-llvm-cases.h baremetal/ir.h baremetal/calc.h $(EFI_BOOTSTRAP)
	$(CC) $(EFI_CFLAGS) -c $< -o $@

.build/ir-llvm.so: .build/ir-llvm-main.o $(filter-out .build/main.o,$(OBJECTS)) $(EFI_BOOTSTRAP)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(filter %.o,$^) -L$(EFI_ROOT)/lib -lgnuefi -o $@

.build/ir-llvm.efi: .build/ir-llvm.so
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc -O pei-x86-64 --subsystem=10 $< $@

.PHONY: test-ir-llvm
test-ir-llvm: .build/ir-llvm.efi
	$(PYTHON) tests/process_qemu.py --image .build/ir-llvm.efi

.build/calc-live.o: tests/calc_live.c neural.c baremetal/calc_model.h baremetal/calc_prompt.h baremetal/calc.h .build/config.h $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -Wno-unused-function -c $< -o $@

.build/calc-live.so: .build/calc-live.o $(filter-out .build/main.o,$(OBJECTS)) $(EFI_BOOTSTRAP)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(filter %.o,$^) -L$(EFI_ROOT)/lib -lgnuefi -o $@

.build/calc-live.efi: .build/calc-live.so
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc -O pei-x86-64 --subsystem=10 $< $@

.PHONY: test-calc-live
CALC_ACCEL ?= tcg
CALC_TIMEOUT ?= 1800
CALC_LOG ?= .build/calc-live.log
test-calc-live: .build/calc-live.efi model.bin
	$(PYTHON) tests/process_qemu.py --image .build/calc-live.efi --model model.bin \
        --memory 2048 --cpus 4 --timeout $(CALC_TIMEOUT) --accel $(CALC_ACCEL) --log "$(CALC_LOG)"

.build/test-calc-model: tests/calc_model.c baremetal/calc.c baremetal/ir.c baremetal/ir.h baremetal/calc_model.h baremetal/calc.h | .build
	$(CC) -O2 -std=c11 -Wall -Wextra -Wpedantic tests/calc_model.c baremetal/calc.c baremetal/ir.c -o $@

# Same calc_live.c and calc_model.h, native Q4 inference + real VM ring3.
# Useful when nested KVM is unavailable and full-model TCG is too slow.
.build/test-calc-native: tests/calc_live.c tests/calc_native_backend.c baremetal/calc.c baremetal/ir.c baremetal/calc.h baremetal/calc_model.h $(CONSOLE_DEPS) linux/runtime.c baremetal/math.c baremetal/cpu.c baremetal/fp.S Makefile | .build
	$(CC) $(HOST_CFLAGS) -DCALC_HOST -Wno-unused-function -pthread tests/calc_live.c tests/calc_native_backend.c baremetal/calc.c baremetal/ir.c linux/runtime.c baremetal/math.c baremetal/cpu.c baremetal/fp.S -o $@

.build/calc-request.o: tests/calc_request_uefi.c .build/calc-request.h baremetal/process.h $(EFI_BOOTSTRAP) | .build
	$(CC) $(EFI_CFLAGS) -c $< -o $@

.build/calc-request.so: .build/calc-request.o $(filter-out .build/main.o,$(OBJECTS)) $(EFI_BOOTSTRAP)
	ld -nostdlib -znocombreloc -shared -Bsymbolic --no-undefined \
		-T $(EFI_ROOT)/lib/elf_x86_64_efi.lds $(EFI_ROOT)/lib/crt0-efi-x86_64.o \
		$(filter %.o,$^) -L$(EFI_ROOT)/lib -lgnuefi -o $@

.build/calc-request.efi: .build/calc-request.so
	$(OBJCOPY) -j .text -j .data -j .rodata -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc -O pei-x86-64 --subsystem=10 $< $@

.PHONY: test-calc-native
test-calc-native: .build/test-calc-native model.bin
	.build/test-calc-native
