# SMOLQ4 v1

All integers and floating-point values are little-endian. The loader accepts
only SmolLM2-135M-Instruct's exact dimensions. This is a deliberately small
private format, not GGUF and not compatible with llama.cpp.

## Header: 256 bytes

| Offset | Type | Value |
| --- | --- | --- |
| 0 | 8 bytes | `SMOLQ4\0\0` |
| 8 | uint32 | version = 1 |
| 12 | uint32 | hidden dimension = 576 |
| 16 | uint32 | intermediate dimension = 1536 |
| 20 | uint32 | layers = 30 |
| 24 | uint32 | query heads = 9 |
| 28 | uint32 | KV heads = 3 |
| 32 | uint32 | vocabulary = 49152 |
| 36 | uint32 | maximum context = 8192 |
| 40 | uint32 | quantization group = 32 |
| 44 | uint32 | BOS = 1 |
| 48 | uint32 | EOS = 2 |
| 52 | uint32 | number of special tokens = 17 |
| 56 | uint32 | number of BPE merges |
| 60 | uint32 | number of Unicode ranges |
| 64 | float32 | RMSNorm epsilon = 1e-5 |
| 68 | float32 | RoPE theta = 100000 |
| 72–255 | bytes | zero padding |

## Tokenizer

Immediately after the header:

1. 256 uint32 byte-to-token IDs. `0xffffffff` marks a byte missing from the
   original vocabulary. Such bytes are skipped, matching HF BPE with no UNK.
2. For each of 49152 tokens: uint32 byte length, then that many raw bytes.
   Regular tokens use the inverse GPT-2 byte alphabet. Special tokens contain
   their literal spelling. Pieces need not individually be valid UTF-8.
3. For each merge, three uint32 values: left ID, right ID, resulting ID.
   Its position in the table is its merge priority. Token IDs are **not** used
   as a substitute for merge priorities.
4. For each Unicode range, three uint32 values: first code point, last code
   point (inclusive), category (1 = Letter, 2 = Number, 3 = White_Space).
   Sorted nonoverlapping ranges; omitted code points have category 0.
5. Zero padding to the next 64-byte boundary.

Special tokens are recognized before normal pre-tokenization. Number code
points are individually isolated first. Remaining spans use the original
GPT-2 ByteLevel splitting rules, followed by BPE with an explicit pair/rank
hash table. This implementation prioritizes small code over worst-case BPE
speed: merging is quadratic within each pre-token. Prompts must be valid UTF-8.

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

1. Embedding `[49152, 576]`, Q4. Also used as the output projection.
2. Final RMSNorm `[576]`, FP32.
3. For each layer 0 through 29: input RMSNorm and post-attention RMSNorm,
   both `[576]` FP32; then Q `[576,576]`, K `[192,576]`, V `[192,576]`,
   O `[576,576]`, gate `[1536,576]`, up `[1536,576]`, down `[576,1536]`, all Q4.

There are no tensor names, directory entries, biases or duplicate LM-head
weights in the binary. The stored model was exported after its source shapes
were validated. The loader validates bounds, dimensions, token IDs and the
expected total file length.

## Forward state

Each token performs embedding lookup, 30 repetitions of attention and MLP
with residual additions, final RMSNorm and tied output projection.
Prompt positions except the last omit the unused output projection.

RoPE rotates the first and second halves of each 64-element Q/K head;
it does not rotate adjacent coordinate pairs. GQA shares one K/V head among
three query heads. Keys and values use `[layer, position, kv_head, coordinate]`
FP32 storage. Attention only reads positions up to and including the current
position, so a separate causal mask is unnecessary.

The UEFI program uses greedy decoding: it selects the largest finite logit.
The compiled application validates intermediate values before decoding.
