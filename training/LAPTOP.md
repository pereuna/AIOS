# AIOS toiselle koneelle ja pienmallien opetuskokeet

## Gitistä kotikoneelle

```sh
git clone https://github.com/pereuna/AIOS.git
cd AIOS
```

Jos repo on jo kloonattu, aja sen hakemistossa `git pull --ff-only`.
Gitissä ovat lähdekoodi, testit, ohjeet ja varmennettu opetusdata:
20 000 opetusesimerkkiä, 1 000 arviointiesimerkkiä sekä asm1:n lisäesimerkit.
Dataa ei tarvitse generoida uudelleen ennen opetusta.

Git ei siirrä `model.bin`-tiedostoa, lähdemallien välimuisteja,
Python-ympäristöjä, lokeja eikä `training/output/`-hakemistoa. Opetus lataa
valitun pohjamallin Hugging Facesta ensimmäisellä käyttökerralla. Myöhemmät
ajot käyttävät paikallista välimuistia. Nykyisen opetuksen adapteri tai
checkpoint pitää kopioida erikseen, jos haluat testata juuri sitä kotona.
Git-päivitys ei itsessään jatka toisella koneella käynnissä olevaa opetusta.

## P2200:n ympäristö

[Quadro P2200](https://www.nvidia.com/content/dam/en-zz/Solutions/design-visualization/productspage/quadro/quadro-desktop/quadro-p2200-datasheet-letter-974207-r4-web.pdf)
on Pascal-kortti, jossa on 5 Gt näyttömuistia. Aloita 0,5B-mallista ja
eräkoosta 1. Sopivuus ja nopeus pitää mitata omalla koneella; näytön ja muiden
ohjelmien muistinkäyttö vaikuttaa käytettävissä olevaan muistiin.

Käytä erillistä Python 3.11 -ympäristöä. Alla oleva asennus valitsee
[PyTorchin virallisen CUDA 12.4 -paketin](https://docs.pytorch.org/get-started/previous-versions/)
versiolle 2.6.0. NVIDIA-ajurin pitää tukea sitä. P2200-kohtainen
requirements-tiedosto pitää tämän Torch-version paikallaan muun asennuksen
aikana. GPU-tarkistus varmistaa myös laskennan, ei pelkkää laitteen löytymistä.
Jos koneella on jo toimiva CUDA-opetusympäristö, voit käyttää sitä ja aloittaa
tarkistuksesta asentamatta paketteja uudelleen.

Linux / WSL:

```sh
python3.11 -m venv .training-venv
. .training-venv/bin/activate
python -m pip install --upgrade pip
python -m pip install torch==2.6.0 --index-url https://download.pytorch.org/whl/cu124
python -m pip install -r training/requirements-p2200.txt
python training/AIOS_TRAINING_WINDOWS.py check
```

Windows PowerShell, suoraan kloonatussa repossa:

```powershell
py -3.11 -m venv .training-venv
.\.training-venv\Scripts\python.exe -m pip install --upgrade pip
.\.training-venv\Scripts\python.exe -m pip install torch==2.6.0 --index-url https://download.pytorch.org/whl/cu124
.\.training-venv\Scripts\python.exe -m pip install -r training/requirements-p2200.txt
.\.training-venv\Scripts\python.exe training/AIOS_TRAINING_WINDOWS.py check
```

Windowsissa ei tarvita `make win` -kopiointia, kun käytät Gitistä kloonattua
repoa. Tarkistus ja opetus toimivat PowerShellistä; asm1-ohjelmien käännös ja
suorituksen arviointi tehdään toistaiseksi Linuxissa tai WSL:ssä.

## Nopeat 0,5B-kokeet

Käytössä on [Qwen2.5-Coder-0.5B-Instruct](https://huggingface.co/Qwen/Qwen2.5-Coder-0.5B-Instruct),
jossa on noin 0,49 miljardia parametria. Se on nykyistä 1,5B-pohjamallia
pienempi ja luonteva lähtökohta asm1:n opetusputken kokeiluun. Nopeutuksen
suuruutta ei ole vielä mitattu P2200:lla.

| Asetus | `small-smoke` | `small-test` |
| --- | --- | --- |
| Optimointiaskeleet | 2 | 50 |
| Opetusesimerkkien otos | 64 | 512 |
| Arviointiesimerkkien otos | 4 | 32 |
| Eräkoko, opetus / arviointi | 1 / 1 | 1 / 1 |
| Gradienttien keräys | 1 | 4 |
| Jakson enimmäispituus | 256 tokenia | 512 tokenia |
| LoRA rank / alpha | 8 / 16 | 8 / 16 |
| Painojen tyyppi / attention | FP16 / SDPA | FP16 / SDPA |
| Tuloshakemisto, `training/output/` alla | `asm1-0.5b-smoke` | `asm1-0.5b-test` |

Linuxissa tai WSL:ssä aktivoidussa ympäristössä:

```sh
python training/train_asm1_lora.py --preset small-smoke
python training/train_asm1_lora.py --preset small-test
```

Windowsissa käytä lokittavaa käynnistintä:

```powershell
.\.training-venv\Scripts\python.exe training/AIOS_TRAINING_WINDOWS.py train --preset small-smoke
.\.training-venv\Scripts\python.exe training/AIOS_TRAINING_WINDOWS.py train --preset small-test
```

Windowsin käynnistin kirjoittaa lokin `training/logs/windows.log`-tiedostoon.
Opetus tallentaa asetukset tiedostoon `asm1_run_config.json`, etenemisen
tiedostoon `progress.json` ja lopputuloksen tiedostoon `asm1_training.json`
valitussa tuloshakemistossa. Etenemistiedosto sisältää ajan ja GPU-muistin
huippukulutuksen. Ensimmäiseen ajoon sisältyy pohjamallin lataus ja datan
esikäsittely; vertaa opetusnopeutta välimuistin täyttymisen jälkeen.

Kaikki erikseen annetut argumentit ohittavat presetin oletuksen, esimerkiksi:

```sh
python training/train_asm1_lora.py --preset small-test --max-steps 20 --output-dir training/output/asm1-0.5b-test-20
```

Käytä kokeille omia tuloshakemistoja. Jos GPU-muisti loppuu, kokeile
`--max-length 256`; lyhyempi raja voi katkaista pitkiä opetusesimerkkejä.
Kahden askeleen savutesti tarkistaa putken toiminnan. Lyhyt opetusajo ja
pienet arviointiotokset eivät vielä osoita lopullisen mallin laatua.

## Vertaa pohjamallia ja adapteria

Linuxissa / WSL:ssä voit arvioida adapterin yhdistämättä sitä pohjamalliin:

```sh
python training/eval_asm1.py \
  --base-model Qwen/Qwen2.5-Coder-0.5B-Instruct \
  --fine-tuned-model training/output/asm1-0.5b-test \
  --dtype float16 --attn-implementation sdpa --batch-size 1 \
  --limit 32 --report training/output/eval-0.5b-test.json
```

Arviointi vertaa asm1:n kääntymistä, suoritusta ja oikeaa tulosta ennen
opetusta ja sen jälkeen. Poista `--limit 32`, kun haluat arvioida koko
1 000 esimerkin joukon. Arviointi tarvitsee C-kääntäjän asm1:n sidontaa varten.
Windowsin adapterihakemisto on kopioitava tähän Linux-repoon tai sen polku
annettava WSL:n kautta, esimerkiksi `/mnt/c/...`.

Projektin nykyinen QWENQ4-vienti, C-inferenssi ja UEFI-ohjelma tukevat vain
Qwen2.5-Coder-1.5B:n rakennetta. Myös `merge_asm1_lora.py` tarkistaa nykyisen
viejän yhteensopivuuden. Käytä 0,5B-kokeissa suoraan adapteria Pythonilla.
0,5B-adapteria ei voi liittää 1,5B-pohjamalliin; onnistunut koe pitää opettaa
erikseen 1,5B-mallille ennen nykyisen USB-version vientiä.
