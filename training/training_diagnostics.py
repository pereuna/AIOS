"""Small numerical checks; no model updates or adapter writes."""

from contextlib import contextmanager, nullcontext


def require_supervised_tokens(labels) -> int:
    count = int((labels[..., 1:] != -100).sum().item())
    if count == 0:
        raise RuntimeError("batch has no supervised next-token labels after masking/truncation")
    return count


class FiniteLossMixin:
    """Abort a Trainer forward pass before backward if its loss is invalid."""

    def compute_loss(self, model, inputs, return_outputs=False, num_items_in_batch=None):
        import torch

        count = require_supervised_tokens(inputs["labels"])
        result = super().compute_loss(
            model, inputs, return_outputs=return_outputs, num_items_in_batch=num_items_in_batch
        )
        loss = result[0] if return_outputs else result
        if not torch.isfinite(loss.detach()).all().item():
            raise RuntimeError(
                f"non-finite forward loss with {count} supervised tokens; stopped before backward. "
                "Run AIOS_TRAINING_WINDOWS.py model-check to locate the failure."
            )
        return result


@contextmanager
def trace_nonfinite_outputs(model):
    """Report the first module whose returned activations contain NaN/Inf."""
    import torch

    def tensors(value):
        if isinstance(value, torch.Tensor):
            yield value
        elif isinstance(value, dict):
            for item in value.values():
                yield from tensors(item)
        elif isinstance(value, (tuple, list)):
            for item in value:
                yield from tensors(item)

    def hook(name):
        def check(module, inputs, output):
            for tensor in tensors(output):
                if tensor.is_floating_point() and not torch.isfinite(tensor).all().item():
                    raise RuntimeError(
                        f"first non-finite module output: {name} ({type(module).__name__}), "
                        f"dtype={tensor.dtype}, shape={tuple(tensor.shape)}"
                    )
        return check

    handles = [module.register_forward_hook(hook(name or "<model>"))
               for name, module in model.named_modules()]
    try:
        yield
    finally:
        for handle in handles:
            handle.remove()


def diagnose_model(trainer) -> int:
    import torch

    model = trainer.model
    device = next(model.parameters()).device
    print(f"Model diagnostic: device={device}, base dtype={model.dtype}", flush=True)
    dtypes = sorted({str(p.dtype) for p in model.parameters() if p.requires_grad})
    print(f"Trainable parameter dtypes: {dtypes}")
    print("Checking loaded weights for NaN/Inf...", flush=True)
    for name, parameter in model.named_parameters():
        if not torch.isfinite(parameter).all().item():
            print(f"FAIL: non-finite loaded parameter: {name}, dtype={parameter.dtype}")
            return 1
    print("Loaded weights: finite", flush=True)

    batches = []
    for name, dataset in (("packed train", trainer.train_dataset), ("eval", trainer.eval_dataset)):
        batch = trainer.data_collator([dataset[0]])
        count = require_supervised_tokens(batch["labels"])
        print(f"{name}: tokens={batch['input_ids'].numel()}, supervised={count}", flush=True)
        batches.append((name, {k: v.to(device) for k, v in batch.items()}))

    original_backend = model.config._attn_implementation
    original_training = model.training
    failures = 0
    # These are forward diagnostics in eval mode, not a substitute for a training smoke test.
    model.eval()
    try:
        for backend in ("eager", "sdpa"):
            model.set_attn_implementation(backend)
            for adapters in (False, True):
                for name, batch in batches:
                    title = f"{backend}, {'LoRA enabled' if adapters else 'base only'}, {name}"
                    print(f"Testing {title}...", flush=True)
                    amp = (torch.autocast("cuda", dtype=torch.float16)
                           if device.type == "cuda" and trainer.args.fp16 else nullcontext())
                    try:
                        with model.disable_adapter() if not adapters else nullcontext():
                            with torch.no_grad(), amp, trace_nonfinite_outputs(model):
                                output = model(**batch, use_cache=False)
                                if not torch.isfinite(output.loss).all().item():
                                    raise RuntimeError("non-finite loss")
                                print(f"PASS: {title}: loss={output.loss.item():.6f}", flush=True)
                                del output
                    except RuntimeError as exc:
                        failures += 1
                        print(f"FAIL: {title}: {exc}", flush=True)
    finally:
        model.set_attn_implementation(original_backend)
        model.train(original_training)
    print(f"Model diagnostic finished: {failures} failed probes; no optimizer steps or adapter saves.")
    return int(failures > 0)
