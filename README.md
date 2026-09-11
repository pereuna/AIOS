# SmolLM2 suoraan UEFI-tikulta

Tässä projektissa on yksi ajettava versio: itsenäinen x86-64 UEFI-ohjelma.
USB-tikulla on pieni `EFI/BOOT/BOOTX64.EFI` ja erillinen `model.bin` tikun
juuressa. Ohjelma lataa nelibittisen SmolLM2-1.7B-Instruct-mallin UEFI:n
tiedostopalveluilla RAMiin. Sen jälkeen levyä ei enää käytetä. Ajettava
toteutus on C:tä ja hieman assembleria: ei Linuxia, C++:aa eikä llama.cpp:tä.

## Kääntäminen

`model.bin` on 964 120 960 tavun (noin 919,46 MiB) SMOLQ4-malli. Sitä ei tallenneta Git-
historiaan. `make` lataa puuttuvat, tiettyyn revisioon lukitut lähdepainot
[Hugging Facesta](https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct),
tarkistaa niiden SHA-256-tiivisteet, kvantisoi `model.bin`-tiedoston paikallisesti
ja kääntää UEFI-tiedoston:

```sh
make
```

Tulos on:

```text
dist/EFI/BOOT/BOOTX64.EFI
dist/model.bin
```

Pelkän mallin voi rakentaa komennolla `make model`. Alkuperäinen noin 3,42 Gt:n
Safetensors-tiedosto jää ignoroituun `.model-source/SmolLM2-1.7B-Instruct`-
välimuistiin, joten sitä ei tarvitse ladata jokaisella käännöskerralla.
Varaa kehityskoneelle noin 7 Gt vapaata levytilaa latausta, muunnosta ja
kopioita varten. Vanhan 135M-version `model.bin` korvataan automaattisesti.

GNU/Linuxissa tarvitaan GCC, binutils, GNU Make, tar, curl, Python 3 ja NumPy.
Järjestelmän Python ja NumPy riittävät; venv-ympäristöä tai pip-asennuksia ei
tarvita. Debianissa/Devuanissa NumPyn paketti on `python3-numpy`.
Tokenizerin Unicode 15.1 -taulukko on mukana tiedostossa
`tools/unicode-15.1.0.txt`, joten Pythonin oman Unicode-tietokannan versio ei
vaikuta malliin. Myös Python 3.14 käy ja tuottaa saman `model.bin`-tiivisteen.

Make käyttää järjestelmän GNU-EFIä, jos se on asennettu. Muuten se hakee
pinnatun x86-64-paketin Debian Snapshotista ja purkaa
sen ilman pääkäyttäjän oikeuksia `.tools/gnu-efi`-hakemistoon. Oman asennuksen
voi valita komennolla `make EFI_ROOT=/polku/prefixiin`.
EFI-kuva tehdään järjestelmän GNU objcopylla käyttäen `pei-x86-64`-formaattia
ja EFI Application -alijärjestelmää (10). Objcopyn voi valita muuttujalla
`OBJCOPY`, esimerkiksi `make OBJCOPY=/usr/bin/objcopy`.
Python ja NumPy ovat vain kehityskoneen työkaluja, eivät USB-version riippuvuuksia.

Kontekstin ja vastauksen oletuspituuden voi asettaa käännösvaiheessa:

```sh
make CONTEXT=2048 TOKENS=128
```

`make clean` poistaa vain `.build`- ja `dist`-hakemistot.
`make test` tarkistaa muunnoksen, pinnatun Unicode-taulukon, EFI-kuvan otsakkeen,
UEFI-tiedostonluvun simuloiduilla firmware-palveluilla ja C-inferenssin
NumPy-vertailua vasten kehityskoneella.

## USB-tikku

Kopioi sekä `dist/EFI` että `dist/model.bin` FAT32-tikun juureen. Lopputulos:

```text
EFI/BOOT/BOOTX64.EFI
model.bin
```

Käynnistä x86-64-kone UEFI-tilassa. Secure Boot pitää poistaa käytöstä, koska
tiedostoa ei ole allekirjoitettu. Suositus on vähintään 4 GiB RAMia.
Oletuskonteksti on 1 024 tokenia: mallipainot ja FP32-KV-välimuisti vievät
yhteensä noin 1,27 GiB, minkä lisäksi tarvitaan ohjelman ja firmwaren muistia.
2 048 tokenilla vastaava määrä on noin 1,65 GiB. Täysi 8 192 tokenin konteksti
ei käytännössä mahdu 4 GiB:n koneeseen. Matriisi-vektorilaskenta käyttää SSE2:ta
ja UEFI MP Services -rajapinnan avulla enintään neljää loogista prosessoria.
Puuttuvalla MP-tuella laskenta toimii yhdellä prosessorilla. Oletus on neljä
workeria; komennolla `/threads 1` voi mitata yhden prosessorin vertailutuloksen.
1.7B on selvästi nykyistä edeltänyttä 135M-mallia raskaampi.

Ohjelma käyttää UEFI:n tiedosto-, näyttö-, näppäimistö-, ajastin-, muistivaraus- ja
sammutustoimintoja. `ExitBootServices()`-kutsua ei tehdä, jotta firmwaren USB-
ja näyttöajurit pysyvät käytössä. Omia levy- tai verkkoajureita ei ole.

Käynnistyksessä pitää näkyä:

```text
UEFI USB -> RAM -> neural.c
Math: SSE2; startup check OK (2026-09-10).
Loading model.bin from USB.............. OK
Checking model CRC32... OK
YOU>
```

Komennot käyttöliittymässä:

| Komento | Toiminto |
| --- | --- |
| `/reset` | Tyhjennä keskustelukonteksti |
| `/tokens N` | Aseta vastauksen enimmäispituus |
| `/stats` | Näytä konteksti, vapaa muistimäärä ja käyntiaika |
| `/threads N` | Valitse 1–4 workeria; 1 käyttää sarjalaskentaa |
| `/selftest` | Aja laskennan tarkistus |
| `/help` | Näytä ohje |
| `/quit` | Sammuta kone UEFI:n kautta |

`Compute:`-rivi näyttää todellisen workerien määrän ja tilan: `MP pool`,
`MP blocking` tai `serial`. Vastauksen lopussa näkyy myös tokenia sekunnissa.
Firmwaren sallimassa pool-tilassa BSP ja enintään kolme AP:tä laskevat
rinnakkain. Jos firmware sallii vain synkroniset MP-kutsut, BSP odottaa ja AP:t
laskevat: neljän loogisen prosessorin koneella tällöin käytössä on enintään
kolme workeria. Lisätiedot ja vertailuohje: [docs/performance.md](docs/performance.md).

Mallin tiedostomuoto on kuvattu [docs/format.md](docs/format.md), testauksen
tulokset [docs/verification.md](docs/verification.md) ja projektin synnyttäneet
keskustelut [docs/keskustelut.md](docs/keskustelut.md).

SmolLM2 soveltuu ensisijaisesti englanninkieliseen kokeiluun. Myös 1.7B-malli
voi toistaa itseään ja antaa vääriä vastauksia. Mallin lähde, revisio,
kvantisointi ja tiiviste ovat tiedostossa `model.json`; lisenssi on
`LICENSE.SmolLM2`.
