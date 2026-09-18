# asm1 LoRA training

For cloning onto another machine and short 0.5B runs on a Quadro P2200,
see [the laptop guide](LAPTOP.md). `--preset small-smoke` and
`--preset small-test` select a smaller base model and separate output
directories; explicit CLI arguments override their defaults. Without a
preset, the existing 1.5B training defaults are unchanged. Small-model
adapters can be evaluated directly in Python; the AIOS C runtime and
exporter currently support only the 1.5B architecture.

This directory fine-tunes `Qwen/Qwen2.5-Coder-1.5B-Instruct` to emit complete
AIOS asm1 programs without putting the asm1 language reference in every REPL
context. Keep the current REPL prompt until the held-out execution score is
good enough; removing it is a separate deployment change.

The data generator has no Python package dependencies. It compiles
`baremetal/asm1.c` as a temporary shared library and requires a host C
compiler. Every accepted reference completion must both compile with that
implementation and return the expected value in the portable asm1 reference
executor.

## 1. Generate verified data

From the repository root:

```sh
python3 training/generate_asm1_dataset.py
```

This deterministically writes 20,000 training rows, 1,000 held-out rows and a
manifest under `training/data/`. Problem instance IDs and natural-language
phrasing templates are disjoint between the splits. To make a quick fixture:

```sh
python3 training/generate_asm1_dataset.py \
  --train-count 200 --eval-count 40 --output-dir /tmp/asm1-data
```

## 2. Train the adapter

Create a GPU environment and install the versioned API family used by the
scripts:

```sh
python3 -m venv .training-venv
. .training-venv/bin/activate
python3 -m pip install -r training/requirements.txt
```

Then train. The initial configuration is LoRA rank 32, alpha 64, completion
tokens only, 512-token sequences and packing enabled. Training uses the same
short AIOS identity as evaluation, with no asm1 language reference:

```sh
accelerate launch training/train_asm1_lora.py
```

Use `--batch-size`, `--gradient-accumulation-steps`, `--epochs` and
`--max-length` to fit the available GPUs. No quantized training is used: the
merged checkpoint must remain a normal BF16 Qwen checkpoint for the AIOS
exporter. The tokenizer is never resized or assigned new tokens. The default
`bfd-requeue` packing works without FlashAttention; `--packing-strategy bfd`
requires a FlashAttention-backed training setup.

### Windows with an existing CUDA PyTorch installation

Run `make win` from the repository in WSL to copy the scripts and verified data
to `C:\temp`. It only copies files; run Python yourself in Windows PowerShell:

```powershell
cd C:\temp
python .\AIOS_TRAINING_WINDOWS.py check
python .\AIOS_TRAINING_WINDOWS.py smoke
```

Every launcher run appends console output, errors and its exit status to
`C:\temp\training\logs\windows.log` (UTF-8). Output remains visible in
PowerShell and is flushed to disk while the run progresses. Earlier runs stay
in the same log, separated by timestamps and process IDs. From WSL, read it at
`/mnt/c/temp/training/logs/windows.log`; Windows Python need not be launched
from WSL. This logging applies when using `AIOS_TRAINING_WINDOWS.py`.

`check` imports the training APIs and checks a small FP16 matrix multiplication
and its gradients on the GPU. `smoke` repeats this check, downloads the base
model if needed, and runs two optimizer steps with batch size 1, 256-token
sequences, SDPA attention and LoRA rank 8. It selects 64 training and 4 evaluation examples
before preprocessing and disables warmup for the two-step test. Its adapter is saved in
`training\output\asm1-smoke`, separately from the real training output.
These settings reduce memory use for the 5 GiB Quadro P2000; the Windows run
must confirm whether they fit. Neither command installs packages.

If CUDA device detection succeeds but the GPU test fails, run this command in a
fresh Windows PowerShell process after updating the bundle with `make win`:

```powershell
python .\AIOS_TRAINING_WINDOWS.py gpu
```

It prints `nvidia-smi` driver, compute-mode, memory and process status, then tests
Torch alone, without importing the Hugging Face training APIs. CUDA allocation,
FP32 arithmetic and FP16 forward/backward failures are reported separately.
The diagnostic runs only on Windows and makes no driver or system configuration
changes. It does not load a model or install packages. Device detection alone
does not establish that CUDA workloads can run; `cudaErrorDevicesUnavailable`
can indicate a restricted compute mode or resources occupied by other workloads
([NVIDIA error reference](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__TYPES.html)).

If training reports NaN/Inf, update the bundle with `make win` and run:

```powershell
python .\AIOS_TRAINING_WINDOWS.py model-check
```

This checks loaded weights and supervised labels, then compares `eager` and
`sdpa` attention with the adapters disabled and enabled on one prepared train
example and one eval example. Hooks identify the first module returning
non-finite activations. These are forward checks in evaluation mode; the
command does not update weights or save an adapter. A passing diagnostic still
needs a successful training smoke test. The model may be downloaded if uncached.
The subset does not reproduce the exact shuffled batch from an earlier full run.

For a full training run, choose the training parameters explicitly, for example:

```powershell
python .\AIOS_TRAINING_WINDOWS.py train --batch-size 1 --eval-batch-size 1 --gradient-accumulation-steps 32 --dtype float16 --attn-implementation sdpa
```

The P2000 diagnostic produced NaN with FP16 eager attention in the first
attention block, while all four SDPA forward probes passed. The smoke test now
uses SDPA; backward and optimizer updates still need confirmation on Windows.

Automatic dtype selection requires native BF16 GPU support; older cards use
FP16. With an FP16 base model, the trainable LoRA parameters are kept in FP32
for stable AMP updates. Padded Qwen embedding rows are preserved without
resizing the tokenizer or the model. `--max-steps N` can limit training
independently of `--epochs`. Empty supervised batches and non-finite forward
losses abort before backward. Logged non-finite metrics are also checked before
the final adapter save. Earlier checkpoints and output from previous runs are
not removed by these checks. `--train-limit` and `--eval-limit` are opt-in
diagnostic subsets; omit them for a full training run.
The old spelling `--packing-strategy bfd_split` is accepted as an alias for
`bfd-requeue`.

Training uses the copied data and does not need a Windows C compiler. The
`generate` and `eval` stages also use the native asm1 C compiler binding;
its current build command is intended for a Unix host. Run those stages in
WSL for now. `make win` does not copy trained outputs back from Windows.

## 3. Merge and verify the checkpoint

```sh
python3 training/merge_asm1_lora.py
```

The command merges `training/output/asm1-lora` into the base model, writes
standard BF16 safetensors to `training/output/asm1-merged`, copies the original
tokenizer unchanged and verifies the output with the strict readers in
`tools/export_model.py`.

The exporter accepts both the original top-level `rope_theta` and the
equivalent Transformers 5 `rope_parameters` representation of unscaled RoPE.
If an older Windows bundle reports `unsupported config value rope_theta=None`
after writing the merged weights, update it with `make win`. The saved weights
can be exported directly with the updated exporter; merging again is not
needed. Scaled or otherwise incompatible RoPE configurations remain rejected.

To export the merged checkpoint to AIOS QWENQ4 directly:

```sh
python3 tools/export_model.py \
  --config training/output/asm1-merged/config.json \
  --tokenizer training/output/asm1-merged/tokenizer.json \
  --weights training/output/asm1-merged/model.safetensors \
  --output model.bin
```

If `--max-shard-size` produced shards, pass
`model.safetensors.index.json` as `--weights`. The normal `make model` target
still installs the pinned upstream checkpoint; it is intentionally not
silently redirected to experimental fine-tuned weights.

## 4. Compare generated programs

```sh
python3 training/eval_asm1.py
```

Evaluation uses the short AIOS assistant identity, not the large asm1 syntax
prompt. For every held-out prompt it extracts the first complete `/asm` call,
compiles it with `baremetal/asm1.c`, executes it with the documented uint32
semantics and compares the value with the verified answer. The terminal and
`training/output/eval_report.json` report compile success, execution success
and correct-result percentages for the base and fine-tuned models plus the
percentage-point changes. `--limit N` is available only for explicit smoke
tests; omit it for the real held-out evaluation.
