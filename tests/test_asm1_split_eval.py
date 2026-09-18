"""Verify evaluation catches wrong programs, missing predictions and stale checkpoints."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "training"))
from asm1_runtime import Asm1Compiler
from eval_asm1_split import score_predictions, score_response
from run_asm1_job import last_checkpoint
from train_asm1_lora import conversation


class SplitEvaluationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = Asm1Compiler()

    def setUp(self):
        self.row = {"instance_id": "held-out", "prompt": "Return seven in asm1", "category": "arithmetic", "expected": 7}

    def test_correct_compiling_but_wrong_and_unrelated_responses(self):
        correct = score_response(self.row, "/asm asm1; li r0 7; exit r0; end", self.compiler)
        wrong = score_response(self.row, "/asm asm1; li r0 8; exit r0; end", self.compiler)
        unrelated = score_response(self.row, "section .text\nmov eax, 7", self.compiler)
        self.assertTrue(correct["correct_result"])
        self.assertTrue(wrong["compile_success"])
        self.assertFalse(wrong["correct_result"])
        self.assertFalse(unrelated["compile_success"])

    def test_runtime_failure_and_infinite_loop_are_not_successes(self):
        for source in ("/asm asm1; li r0 7; li r1 0; udiv r0 r1; exit r0; end",
                       "/asm asm1; label l0; jmp l0; exit r0; end"):
            result = score_response(self.row, source, self.compiler)
            self.assertTrue(result["compile_success"])
            self.assertFalse(result["execution_success"])

    def test_missing_duplicate_and_changed_prompts_are_rejected(self):
        good = {"instance_id": "held-out", "prompt": self.row["prompt"], "response": "/asm asm1; li r0 7; exit r0; end"}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "predictions.jsonl"
            for predictions in ([good, good], [dict(good, instance_id="different")], [dict(good, prompt="different")]):
                path.write_text("".join(json.dumps(row) + "\n" for row in predictions))
                with self.assertRaises(ValueError):
                    score_predictions([self.row], path, self.compiler)

    def test_resume_ignores_incomplete_latest_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            old = Path(directory) / "checkpoint-25"
            new = Path(directory) / "checkpoint-50"
            old.mkdir()
            new.mkdir()
            for name in ("trainer_state.json", "adapter_model.safetensors", "optimizer.pt", "scheduler.pt", "rng_state.pth"):
                (old / name).touch()
            (new / "adapter_model.safetensors").touch()
            self.assertEqual(last_checkpoint(directory), str(old))

    def test_optional_system_message_never_removes_user_or_answer(self):
        row = {"prompt": "What is asm1?", "completion": "A small AIOS language."}
        full = conversation(row, "identity", 0)
        empty = conversation(row, "identity", 1)
        self.assertEqual(full["prompt"][0]["role"], "system")
        self.assertEqual(empty["prompt"], [{"role": "user", "content": row["prompt"]}])
        self.assertEqual(full["completion"], empty["completion"])


if __name__ == "__main__":
    unittest.main()
