# LLM-to-LLVM-IR-kokeilun opetusvalmius

Projektin nykyinen kokeilu on [LLM → LLVM SSA IR → x86-64 → ring3 -laskin](../docs/calc.md).
Se käyttää olemassa olevaa Q4 `model.bin`:ä ja `calc_prompt.h`-järjestelmäohjetta.
Mallipainoja tai tokenizeria ei muuteta. Assembler-opetus ei ole tämän kokeen
seuraava vaihe.

Yleiskäyttöiset `train_lora.py`, `merge_lora.py`, GPU-diagnostiikka ja painojen
vientityökalut säilyvät mahdollista myöhempää hienosäätöä varten. Vanha
kokeilukielen aineisto ja sen arvioijat on poistettu. Uutta opetusaineistoa ei
vielä toimiteta.

Ennen opetusta mitataan oikean mallin `/calc`-kutsut ja sanallisista kysymyksistä
löydetyt laskuosuudet. Virheet jaetaan työkalun valintaan, operandien/operaation
tunnistamiseen, SSA-ohjelman oikeellisuuteen ja lopullisen vastauksen uskollisuuteen.
QEMU:n CPU-laskennan oikeellisuus tarkistetaan erikseen.

Mahdollinen aineisto sisältää `prompt`/`completion`-JSONL-pareja. Completion on
kokonainen rajatun osajoukon `define i64 @calc() { ... }` -funktio tai tavallinen
vastaus, kun laskua ei tarvita. IR-esimerkkien tulee käyttää nimettyjä SSA-arvoja,
vähintään yhtä `add`-, `sub`-, `mul`- tai `sdiv`-operaatiota ja palauttaa viimeinen
tulos viittauksena, ei valmiiksi laskettuna vakiona.
Lisäksi tarvitaan keskusteluesimerkkejä työkalutuloksen hyödyntämisestä ja
virheellisen kutsun korjaamisesta. Train/dev/test jaetaan tehtävä- ja
muotoiluperheittäin, jotta eri vakiot samasta pohjasta eivät ole ainoa
riippumattomuuskriteeri. Jokainen IR-esimerkki tarkistetaan oikealla LLVM-
parserilla sekä AIOS-kääntäjän ring3-ajolla.

```sh
python3 training/train_lora.py --train-file /path/train.jsonl --eval-file /path/dev.jsonl
python3 training/merge_lora.py --adapter training/output/calc-lora
```

Asenna `training/requirements.txt` erilliseen GPU-ympäristöön. Presetit
`small-smoke` ja `small-test` testaavat putkea 0,5B-mallilla. Windows-käynnistin
säilyttää ympäristödiagnostiikan ja train/merge-vaiheet. `make win` kopioi
putken työkalut; aineisto toimitetaan erikseen.

Hienosäätö aloitetaan alkuperäisistä BF16/FP16-painoista, ei Q4-ajomallista tai
vanhan kokeilukielen adapterista. Tokenizer pidetään muuttumattomana. Adapteri
yhdistetään checkpointiin ennen `tools/export_model.py`- ja
`tools/split_model.py`-vientiä. Myös viety Q4-malli arvioidaan samalla live-kokeella.
Trainerin eval loss ei korvaa työkalun valinnan ja ohjelmasuorituksen arviointia.
