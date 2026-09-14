# asm1 LoRA training

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
`bfd_split` packing works without FlashAttention; `--packing-strategy bfd`
requires a FlashAttention-backed training setup.

## 3. Merge and verify the checkpoint

```sh
python3 training/merge_asm1_lora.py
```

The command merges `training/output/asm1-lora` into the base model, writes
standard BF16 safetensors to `training/output/asm1-merged`, copies the original
tokenizer unchanged and verifies the output with the strict readers in
`tools/export_model.py`.

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
