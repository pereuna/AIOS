"""Numerical failure checks on small CPU tensors; no downloaded models."""

from pathlib import Path
from types import SimpleNamespace
import sys
import unittest

import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "training"))
from training_diagnostics import FiniteLossMixin, require_supervised_tokens, trace_nonfinite_outputs


class ParentTrainer:
    def compute_loss(self, model, inputs, return_outputs=False, num_items_in_batch=None):
        self.calls += 1
        return (self.loss, {"loss": self.loss}) if return_outputs else self.loss


class CheckedTrainer(FiniteLossMixin, ParentTrainer):
    def __init__(self, loss):
        self.loss = loss
        self.calls = 0


class NumericalChecks(unittest.TestCase):
    def test_shifted_labels_need_a_target_after_first_position(self):
        for labels in ([[3, -100]], [[-100, -100]]):
            trainer = CheckedTrainer(torch.tensor(1.0))
            with self.assertRaisesRegex(RuntimeError, "no supervised"):
                trainer.compute_loss(None, {"labels": torch.tensor(labels)})
            self.assertEqual(trainer.calls, 0)
        self.assertEqual(require_supervised_tokens(torch.tensor([[-100, 3, 4, -100]])), 2)

    def test_nonfinite_loss_never_reaches_backward(self):
        for value in (float("nan"), float("inf"), -float("inf")):
            for return_outputs in (False, True):
                weight = torch.tensor(value, requires_grad=True)
                trainer = CheckedTrainer(weight * 2)
                with self.assertRaisesRegex(RuntimeError, "stopped before backward"):
                    result = trainer.compute_loss(None, {"labels": torch.tensor([[-100, 2]])}, return_outputs)
                    (result[0] if return_outputs else result).backward()
                self.assertIsNone(weight.grad)

    def test_finite_loss_preserves_gradient_and_return_contract(self):
        weight = torch.tensor(2.0, requires_grad=True)
        trainer = CheckedTrainer(weight * 3)
        loss, outputs = trainer.compute_loss(None, {"labels": torch.tensor([[-100, 2]])}, True)
        self.assertIs(outputs["loss"], loss)
        loss.backward()
        self.assertEqual(weight.grad.item(), 3)

    def test_trace_identifies_first_bad_module_and_removes_hooks(self):
        class Invalid(torch.nn.Module):
            def forward(self, x):
                return x / 0

        model = torch.nn.Sequential(torch.nn.Identity(), Invalid(), torch.nn.Identity())
        with self.assertRaisesRegex(RuntimeError, "1 \\(Invalid\\)"):
            with trace_nonfinite_outputs(model):
                model(torch.ones(2))
        self.assertTrue(all(not module._forward_hooks for module in model.modules()))

    def test_finite_trace_leaves_outputs_unchanged(self):
        model = torch.nn.Linear(2, 2)
        x = torch.ones(1, 2)
        expected = model(x)
        with trace_nonfinite_outputs(model):
            torch.testing.assert_close(model(x), expected)
        self.assertTrue(all(not module._forward_hooks for module in model.modules()))


if __name__ == "__main__":
    unittest.main()
