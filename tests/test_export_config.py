"""Preserve QWENQ4 RoPE semantics across Hugging Face config formats."""

from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools import export_model as exporter


def legacy_config():
    return {
        "model_type": "qwen2",
        "hidden_act": "silu",
        "hidden_size": 1536,
        "intermediate_size": 8960,
        "num_hidden_layers": 28,
        "num_attention_heads": 12,
        "num_key_value_heads": 2,
        "vocab_size": 151936,
        "max_position_embeddings": 32768,
        "bos_token_id": 151643,
        "eos_token_id": 151645,
        "rms_norm_eps": 1e-6,
        "rope_theta": 1000000.0,
        "tie_word_embeddings": True,
        "use_sliding_window": False,
    }


def modern_config():
    config = legacy_config()
    theta = config.pop("rope_theta")
    config["rope_parameters"] = {"rope_type": "default", "rope_theta": theta}
    config["layer_types"] = ["full_attention"] * 28
    return config


class ExportConfigTests(unittest.TestCase):
    def test_legacy_and_modern_configs_have_identical_header_parameters(self):
        legacy = exporter.check_config(legacy_config())
        source = modern_config()
        before = deepcopy(source)
        modern = exporter.check_config(source)
        for name in ("bos_token_id", "eos_token_id", "rms_norm_eps", "rope_theta"):
            self.assertEqual(modern[name], legacy[name])
        self.assertEqual(source, before)

    def test_full_rotary_factor_is_supported(self):
        config = modern_config()
        config["rope_parameters"]["partial_rotary_factor"] = 1.0
        self.assertEqual(exporter.check_config(config)["rope_theta"], 1000000.0)

    def test_unsupported_rope_semantics_are_rejected(self):
        for rope in (
            {},
            [],
            {"rope_type": "linear", "rope_theta": 1000000.0, "factor": 2.0},
            {"rope_type": "default", "rope_theta": 10000.0},
            {"rope_type": "default"},
            {"rope_type": "default", "rope_theta": 1000000.0, "factor": 2.0},
            {"rope_type": "default", "rope_theta": 1000000.0, "partial_rotary_factor": 0.5},
            {"full_attention": {"rope_type": "default", "rope_theta": 1000000.0}},
        ):
            with self.subTest(rope=rope), self.assertRaises(ValueError):
                config = modern_config()
                config["rope_parameters"] = rope
                exporter.check_config(config)

    def test_conflicting_legacy_and_modern_values_are_rejected(self):
        config = modern_config()
        config["rope_theta"] = 10000.0
        with self.assertRaisesRegex(ValueError, "conflicting rope_theta"):
            exporter.check_config(config)
        config["rope_theta"] = 1000000.0
        exporter.check_config(config)

    def test_legacy_scaling_and_missing_theta_are_still_rejected(self):
        config = legacy_config()
        config["rope_scaling"] = {"type": "linear", "factor": 2.0}
        with self.assertRaisesRegex(ValueError, "rope_scaling"):
            exporter.check_config(config)
        config = legacy_config()
        config.pop("rope_theta")
        with self.assertRaisesRegex(ValueError, "rope_theta"):
            exporter.check_config(config)

    def test_sliding_attention_and_architecture_changes_are_rejected(self):
        config = modern_config()
        config["layer_types"][-1] = "sliding_attention"
        with self.assertRaisesRegex(ValueError, "layer_types"):
            exporter.check_config(config)
        config = modern_config()
        config["num_hidden_layers"] = 29
        with self.assertRaisesRegex(ValueError, "num_hidden_layers"):
            exporter.check_config(config)

    def test_cli_passes_normalized_theta_to_binary_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "model.bin"
            output.write_bytes(b"fixture")
            argv = ["export_model", "--config", "config.json", "--tokenizer", "tokenizer.json",
                    "--weights", "model.safetensors", "--output", str(output)]
            with patch.object(sys, "argv", argv), patch.object(exporter, "load_json",
                    side_effect=[modern_config(), {}]), patch.object(exporter, "SafeTensors"), \
                    patch.object(exporter, "write_model") as write:
                exporter.main()
            self.assertEqual(write.call_args.args[1]["rope_theta"], 1000000.0)


if __name__ == "__main__":
    unittest.main()
