# SmolLM2 suoraan UEFI-tikulta

Tässä projektissa on yksi ajettava versio: itsenäinen x86-64 UEFI-ohjelma.
USB-tikulla on yksi tiedosto, `EFI/BOOT/BOOTX64.EFI`. Se sisältää kernelin,
tokenisoijan ja nelibittisen SmolLM2-135M-Instruct-mallin. Käynnistyksen jälkeen
ohjelma toimii kokonaan RAMissa ilman käyttöjärjestelmää, levyajuria, verkkoa tai
muita tikun tiedostoja.

## Kääntäminen

`model.bin` on valmis 73,35 MiB:n SMOLQ4-malli. Sitä ei tallenneta Git-
historiaan. `make` lataa puuttuvat, tiettyyn revisioon lukitut lähdepainot
[Hugging Facesta](https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct),
tarkistaa niiden SHA-256-tiivisteet, kvantisoi `model.bin`-tiedoston paikallisesti
ja kääntää UEFI-tiedoston:

```sh
make
```

Tulos on:

```text
dist/EFI/BOOT/BOOTX64.EFI
```

Pelkän mallin voi rakentaa komennolla `make model`. Alkuperäinen 257 MiB:n
Safetensors-tiedosto jää ignoroituun `.model-source`-välimuistiin, joten sitä ei
tarvitse ladata jokaisella käännöskerralla.

GNU/Linuxissa tarvitaan GCC, binutils, GNU Make, tar, curl ja Python 3. Mallin
paikalliseen rakentamiseen tarvitaan lisäksi NumPy ja Unicode 15.1 -tiedot
(esimerkiksi Python 3.13). Make käyttää järjestelmän GNU-EFIä, jos se on
asennettu. Muuten se hakee pinnatun x86-64-paketin Debian Snapshotista ja purkaa
sen ilman pääkäyttäjän oikeuksia `.tools/gnu-efi`-hakemistoon. Oman asennuksen
voi valita komennolla `make EFI_ROOT=/polku/prefixiin`.

Kontekstin ja vastauksen oletuspituuden voi asettaa käännösvaiheessa:

```sh
make CONTEXT=2048 TOKENS=128
```

`make clean` poistaa vain `.build`- ja `dist`-hakemistot.

## USB-tikku

Alusta tikku FAT32-muotoon ja kopioi `dist/EFI` tikun juureen. Lopputulos:

```text
EFI/
└── BOOT/
    └── BOOTX64.EFI
```

Käynnistä x86-64-kone UEFI-tilassa. Secure Boot pitää poistaa käytöstä, koska
tiedostoa ei ole allekirjoitettu. Varaa vähintään 512 MiB RAMia; 1 GiB on hyvä
käytännön minimi eri firmwareille.

Ohjelma käyttää UEFI:n näyttö-, näppäimistö-, ajastin-, muistivaraus- ja
sammutustoimintoja. `ExitBootServices()`-kutsua ei tehdä, jotta firmwaren USB-
ja näyttöajurit pysyvät käytössä. Omia levy- tai verkkoajureita ei ole.

Käynnistyksessä pitää näkyä:

```text
UEFI USB -> RAM -> neural.c
Math: SSE2; startup check OK (2026-09-10).
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

SmolLM2 soveltuu ensisijaisesti englanninkieliseen kokeiluun. Pieni 135M-malli
voi toistaa itseään ja antaa vääriä vastauksia. Mallin lähde, revisio,
kvantisointi ja tiiviste ovat tiedostossa `model.json`; lisenssi on
`LICENSE.SmolLM2`.
# AIOS
