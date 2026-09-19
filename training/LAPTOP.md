# Pienen GPU:n opetuskokeet

Yleiskäyttöiset GPU-diagnostiikka, pienet opetuspresetit ja LoRA-putki säilyvät.
Asenna `training/requirements.txt` erilliseen Python-ympäristöön. Windowsissa
`python training/AIOS_TRAINING_WINDOWS.py check` tarkistaa ympäristön ja
`gpu` GPU:n toiminnan. `model-check` tekee opetusmallin diagnostiikan.

Uusi varmennettu LLM-to-LLVM-IR-aineisto pitää valmistella ennen opetusta; vanhan
kokeilukielen aineisto on poistettu. JSONL-rivillä on `prompt` ja `completion`.

```sh
python training/train_lora.py --preset small-smoke
python training/train_lora.py --preset small-test
```

Presetit käyttävät 0,5B-mallia, erillisiä tuloshakemistoja ja rajattuja
opetusmääriä. Ne testaavat putkea; tulos ei osoita luotettavaa laskennan tunnistamista.
1,5B-tuotantomallin vienti edellyttää sen omaa checkpointia ja arkkitehtuuria.
Katso [kokeilusuunnitelma](README.md).
