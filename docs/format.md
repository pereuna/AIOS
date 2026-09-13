# QWENQ4 v1

All integers and floating-point values are little-endian. The loader accepts
only Qwen2.5-Coder-1.5B-Instruct's exact dimensions. This is a deliberately small
private format, not GGUF and not compatible with llama.cpp.

## Header: 256 bytes

| Offset | Type | Value |
| --- | --- | --- |
| 0 | 8 bytes | `QWENQ4\0\0` |
| 8 | uint32 | version = 1 |
| 12 | uint32 | hidden dimension = 1536 |
| 16 | uint32 | intermediate dimension = 8960 |
| 20 | uint32 | layers = 28 |
| 24 | uint32 | query heads = 12 |
| 28 | uint32 | KV heads = 2 |
| 32 | uint32 | vocabulary = 151936 |
| 36 | uint32 | maximum context = 32768 (runtime capped at 8192) |
| 40 | uint32 | quantization group = 32 |
| 44 | uint32 | BOS = 151643 (endoftext) |
| 48 | uint32 | EOS = 151645 (im_end) |
| 52 | uint32 | number of added tokens = 22 |
| 56 | uint32 | number of BPE merges |
| 60 | uint32 | number of Unicode ranges |
| 64 | float32 | RMSNorm epsilon = 1e-6 |
| 68 | float32 | RoPE theta = 1000000 |
| 72–255 | bytes | zero padding |

## Tokenizer

Immediately after the header:

1. 256 uint32 byte-to-token IDs. `0xffffffff` marks a byte missing from the
   original vocabulary. Such bytes are skipped, matching HF BPE with no UNK.
2. For each of 151936 tokens: uint32 byte length, then that many raw bytes.
   Regular tokens use the inverse GPT-2 byte alphabet. Special tokens contain
   their literal spelling. Pieces need not individually be valid UTF-8.
3. For each merge, three uint32 values: left ID, right ID, resulting ID.
   Its position in the table is its merge priority. Token IDs are **not** used
   as a substitute for merge priorities.
4. For each Unicode range, three uint32 values: first code point, last code
   point (inclusive), category (1 = Letter, 2 = Number, 3 = White_Space).
   Sorted nonoverlapping ranges; omitted code points have category 0.
5. Zero padding to the next 64-byte boundary.

The 807 Unicode ranges are pinned to Unicode 15.1.0 in
`tools/unicode-15.1.0.txt`. The exporter reads this table rather than the build
host's Unicode database, preserving tokenization and the model's SHA-256
across Python versions.

Added tokens 151643–151664 are recognized before normal pre-tokenization.
The first 14 are special control tokens; the last eight are regular added
code/tool markers. Ordinary spans are NFC-normalized using the committed
Unicode 15.1 tables in `baremetal/nfc_data.h`. The upstream pre-tokenizer
isolates case-insensitive contractions, letter spans with an optional prefix,
individual numbers, punctuation (including following CR/LF), and whitespace
runs before ByteLevel BPE. Merging is quadratic within each pre-token.
Prompts must be valid UTF-8.

The checkpoint has 151936 vocabulary rows; the BPE vocabulary defines 151643
IDs and the tokenizer adds 22. The final 271 rows have placeholder pieces in
the file and are rejected as output. Control tokens endoftext (151643) and
im_end (151645) end generation. Other special controls are rejected; the eight
non-special added markers remain printable. There are 151387 merges.
Weights begin at byte 3416256.

## Weights

All matrices are row-major, shape `[output, input]`. Each row is split into
32-element groups. An 18-byte group contains:

```text
bytes 0..1:  FP16 scale s
bytes 2..17: 16 packed bytes

w[i]    = s * ((packed[i] & 15) - 8),  i = 0..15
w[i+16] = s * ((packed[i] >> 4) - 8),  i = 0..15
```

Quantization computes `s = fp16(max(abs(w)) / 7)`, then rounds `w / float32(s)`
to the nearest even integer and clamps to `[-7, 7]`. A zero-scale group decodes
to zero. The unused `-8` level is representable by the reader. FP16 scale
rounding happens before quantizing the weights, so exporter and decoder use
the same scale.

Weight order:

1. Embedding `[151936,1536]`, Q4, also used as the output projection.
2. Final RMSNorm `[1536]`, FP32.
3. For each layer 0 through 27: input RMSNorm and post-attention RMSNorm,
   both `[1536]` FP32; Q bias `[1536]`, K and V biases `[256]`, all FP32;
   then Q `[1536,1536]`, K and V `[256,1536]`, O `[1536,1536]`,
   gate and up `[8960,1536]`, down `[1536,8960]`, all Q4.

Source shapes and BF16 dtypes are validated in the single Safetensors file.
The loader validates bounds, dimensions, token IDs and total file length.
There is no separately stored LM head: the checkpoint ties it to embeddings.

## Forward state

Each token performs embedding lookup, 28 repetitions of attention and MLP
with residual additions, final RMSNorm and tied output projection.
Prompt positions except the last omit the unused output projection.
Q/K/V biases are added after their linear projections, before RoPE and KV caching.

RoPE rotates the first and second halves of each 128-element Q/K head;
it does not rotate adjacent coordinate pairs. Six query heads share each K/V
head (grouped-query attention). Keys and values use
`[layer, position, kv_head, coordinate]`
FP32 storage. Attention only reads positions up to and including the current
position, so a separate causal mask is unnecessary.

The UEFI program uses greedy decoding: it selects the largest finite logit.
The compiled application validates intermediate values before decoding.

## USB loading

The 872,253,632-byte model lives at `model.000` on the EFI application's own
boot volume. The distribution helper retains support for parts of at most
2 GiB, but Qwen needs only one. UEFI Simple File System and File protocols
load it into one AllocatePages allocation, reading at most 1 MiB per call
and accepting short reads until the expected size is reached. Unexpected EOF
and size mismatches are fatal. Files close before CRC32 and inference.

Only the expected CRC32 and context/output defaults are compiled into the EFI
image; there is no embedded model or additional boot-payload format. The build
also verifies the model's pinned SHA-256 before generating those settings.
