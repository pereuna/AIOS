"""Fast tests for asm1 dataset compilation, execution and split isolation."""

from collections import Counter
import hashlib
import json
from pathlib import Path
import random
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "training"))

from asm1_runtime import ASM1_OK, Asm1Compiler, execute, extract_program, verify_program
from generate_asm1_dataset import GENERATORS, make_split


class Asm1TrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = Asm1Compiler()

    def test_existing_compiler_and_reference_execution_agree(self):
        source = (
            "asm1; input 48 18; ld r0 0; ld r1 1; label l0; jz r1 l1; "
            "mov r2 r0; umod r2 r1; mov r0 r1; mov r1 r2; jmp l0; "
            "label l1; exit r0; end"
        )
        compiled = self.compiler.compile(source)
        self.assertEqual(compiled.status, ASM1_OK)
        self.assertTrue(compiled.complete)
        self.assertEqual(execute(source).value, 6)

    def test_every_generator_category_produces_verified_programs(self):
        rng = random.Random(7)
        for name, generator in GENERATORS.items():
            for _ in range(12):
                candidate = generator(rng)
                with self.subTest(category=name, key=candidate.problem_key):
                    verify_program(self.compiler, candidate.source, candidate.expected)

    def test_splits_have_no_problem_or_phrasing_template_overlap(self):
        rng = random.Random(11)
        used = set()
        train = make_split("train", 80, rng, self.compiler, used)
        evaluation = make_split("eval", 40, rng, self.compiler, used)
        train_ids = {row["instance_id"] for row in train}
        eval_ids = {row["instance_id"] for row in evaluation}
        self.assertFalse(train_ids & eval_ids)
        self.assertFalse(
            {row["template_id"] for row in train}
            & {row["template_id"] for row in evaluation}
        )
        self.assertEqual({row["category"] for row in train}, set(GENERATORS))
        for row in train + evaluation:
            source = extract_program(row["completion"])
            self.assertIsNotNone(source)
            verify_program(self.compiler, source, row["expected"])

    def test_generated_dataset_matches_manifest(self):
        data = ROOT / "training" / "data"
        manifest = json.loads((data / "manifest.json").read_text(encoding="utf-8"))
        ids = {}
        templates = {}
        for split in ("train", "eval"):
            path = data / manifest[split]["file"]
            content = path.read_bytes()
            rows = [json.loads(line) for line in content.splitlines()]
            self.assertEqual(len(rows), manifest[split]["examples"])
            self.assertEqual(hashlib.sha256(content).hexdigest(), manifest[split]["sha256"])
            self.assertEqual(
                dict(sorted(Counter(row["category"] for row in rows).items())),
                manifest[split]["categories"],
            )
            ids[split] = {row["instance_id"] for row in rows}
            templates[split] = {row["template_id"] for row in rows}
            self.assertEqual(len(ids[split]), len(rows))
        self.assertFalse(ids["train"] & ids["eval"])
        self.assertFalse(templates["train"] & templates["eval"])


if __name__ == "__main__":
    unittest.main()
