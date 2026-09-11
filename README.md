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

GNU/Linuxissa tarvitaan GCC, binutils, GNU Make, tar, curl ja Python 3. Mallin
paikalliseen rakentamiseen tarvitaan lisäksi NumPy ja Unicode 15.1 -tiedot
(esimerkiksi Python 3.13). Make käyttää järjestelmän GNU-EFIä, jos se on
asennettu. Muuten se hakee pinnatun x86-64-paketin Debian Snapshotista ja purkaa
sen ilman pääkäyttäjän oikeuksia `.tools/gnu-efi`-hakemistoon. Oman asennuksen
voi valita komennolla `make EFI_ROOT=/polku/prefixiin`.
Python ja NumPy ovat vain kehityskoneen työkaluja, eivät USB-version riippuvuuksia.

Kontekstin ja vastauksen oletuspituuden voi asettaa käännösvaiheessa:

```sh
make CONTEXT=2048 TOKENS=128
```

`make clean` poistaa vain `.build`- ja `dist`-hakemistot.
`make test` tarkistaa muunnoksen, UEFI-tiedostonluvun simuloiduilla
firmware-palveluilla ja C-inferenssin NumPy-vertailua vasten kehityskoneella.

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
ei käytännössä mahdu 4 GiB:n koneeseen. Laskenta käyttää yhtä CPU-ydintä ja
SSE2:ta; 1.7B on selvästi nykyistä edeltänyttä 135M-mallia raskaampi.

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
| `/selftest` | Aja laskennan tarkistus |
| `/help` | Näytä ohje |
| `/quit` | Sammuta kone UEFI:n kautta |

Mallin tiedostomuoto on kuvattu [docs/format.md](docs/format.md), testauksen
tulokset [docs/verification.md](docs/verification.md) ja projektin synnyttäneet
keskustelut [docs/keskustelut.md](docs/keskustelut.md).

SmolLM2 soveltuu ensisijaisesti englanninkieliseen kokeiluun. Myös 1.7B-malli
voi toistaa itseään ja antaa vääriä vastauksia. Mallin lähde, revisio,
kvantisointi ja tiiviste ovat tiedostossa `model.json`; lisenssi on
`LICENSE.SmolLM2`.
