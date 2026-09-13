# Qwen2.5-Coder suoraan UEFI-tikulta

Tässä projektissa on yksi ajettava versio: itsenäinen x86-64 UEFI-ohjelma.
USB-tikulla on pieni `EFI/BOOT/BOOTX64.EFI` ja mallitiedosto `model.000` tikun
juuressa. Ohjelma lataa nelibittisen Qwen2.5-Coder-1.5B-Instruct-mallin UEFI:n
tiedostopalveluilla RAMiin. Sen jälkeen levyä ei enää käytetä. Ajettava
toteutus on C:tä ja hieman assembleria: ei Linuxia, C++:aa eikä llama.cpp:tä.

## Kääntäminen

`model.bin` on 872 253 632 tavun (noin 831,85 MiB) QWENQ4-malli. Sitä ei tallenneta Git-
historiaan. `make` lataa puuttuvat, tiettyyn revisioon lukitut lähdepainot
[Hugging Facesta](https://huggingface.co/Qwen/Qwen2.5-Coder-1.5B-Instruct),
tarkistaa niiden SHA-256-tiivisteet, kvantisoi `model.bin`-tiedoston paikallisesti
ja kääntää UEFI-tiedoston:

```sh
make
```

Tulos on:

```text
dist/EFI/BOOT/BOOTX64.EFI
dist/model.000
```

Pelkän mallin voi rakentaa komennolla `make model`. Alkuperäinen noin 3,09 Gt:n
BF16-painotiedosto jää ignoroituun
`.model-source/Qwen2.5-Coder-1.5B-Instruct`-
välimuistiin, joten sitä ei tarvitse ladata jokaisella käännöskerralla.
Varaa kehityskoneelle noin 6 Gt vapaata levytilaa latausta, muunnosta ja
kopioita varten. Vanhan malliversion `model.bin` korvataan automaattisesti.

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
make CONTEXT=2048 TOKENS=512
```

Nämä ovat myös oletukset. `TOKENS` on yhden vastauksen tokenbudjetti,
mukaan lukien mallin tulostama päättely.

`make clean` poistaa vain `.build`- ja `dist`-hakemistot.
`make test` tarkistaa muunnoksen, pinnatun Unicode-taulukon, EFI-kuvan otsakkeen,
UEFI-tiedostonluvun simuloiduilla firmware-palveluilla ja C-inferenssin
NumPy-vertailua vasten kehityskoneella. `make test-model-uefi` tarkistaa
mallitiedoston latauksen, CRC32:n ja neljän tokenin päättelyn
QEMU/OVMF:ssä (KVM, 2 GiB VM-muistia).

## USB-tikku

Valmiin `dist`-hakemiston voi asentaa irrotettavalle FAT32-tikulle apuohjelmalla:

```sh
tools/make_usb.sh /dev/sda
```

Komento käyttää olemassa olevaa `/dev/sda1`-osiota. Jos tikku pitää alustaa,
käytä erikseen tuhoavaa komentoa `sudo tools/make_usb.sh --format --yes /dev/sda`.
Työkalu vaatii irrotettavan levyn (`lsblk RM=1`), irrottaa sen lopuksi ja vertaa
ennen jokaista kopiointia kokoa sekä SHA-256-tiivistettä. Samanlaiset mallitiedoston osat
ohitetaan kokonaan.

Kopioi `dist/EFI` ja `dist/model.000` FAT32-tikun juureen.
Qwenin Q4-painot mahtuvat yhteen FAT32-yhteensopivaan tiedostoon. Lopputulos:

```text
EFI/BOOT/BOOTX64.EFI
model.000
```

Käynnistä x86-64-kone UEFI-tilassa. Secure Boot pitää poistaa käytöstä, koska
tiedostoa ei ole allekirjoitettu. Suositus on vähintään 2 GiB RAMia ja 2 Gt:n USB-tikku.
Oletuskonteksti on 2 048 tokenia: mallipainot ja FP32-KV-välimuisti vievät
yhteensä noin 944 MiB, minkä lisäksi tarvitaan ohjelman ja firmwaren muistia.
1 024 tokenilla vastaava määrä on noin 888 MiB ja 8 192 tokenilla noin 1,25 GiB.
Lähdemalli tukee 32 768 tokenia, mutta tämän UEFI-toteutuksen konteksti on
rajattu 8 192 tokeniin. Matriisi-vektorilaskenta käyttää
automaattisesti AVX2:ta, kun suorittavan ytimen CPUID ja XCR0 sallivat sen;
muuten käytetään SSE2:ta. UEFI MP Services -rajapinnalla laskenta käyttää
enintään neljää loogista prosessoria.
Puuttuvalla MP-tuella laskenta toimii yhdellä prosessorilla. Oletus on neljä
workeria; komennolla `/threads 1` voi mitata yhden prosessorin vertailutuloksen.
Qwenin Q4-painot vievät noin 81 % vähemmän tilaa kuin aiemman Seed-Coder-8B:n
painot. Myös KV-välimuisti on selvästi pienempi.

Ohjelma käyttää UEFI:n tiedosto-, näyttö-, näppäimistö-, ajastin-, muistivaraus- ja
sammutustoimintoja. `ExitBootServices()`-kutsua ei tehdä, jotta firmwaren USB-
ja näyttöajurit pysyvät käytössä. Omia levy- tai verkkoajureita ei ole.

Käynnistyksessä pitää näkyä:

```text
UEFI USB -> RAM -> neural.c
Math: SSE2; startup check OK (2026-09-10).
Loading model parts from USB.............. OK
Checking model CRC32... OK
YOU>
```

Komennot käyttöliittymässä:

| Komento | Toiminto |
| --- | --- |
| `/reset` | Tyhjennä keskustelukonteksti |
| `/tokens N` | Aseta kysymyksen yhteinen generointibudjetti (oletus 512) |
| `/stats` | Näytä konteksti, vapaa muistimäärä ja käyntiaika |
| `/threads N` | Valitse 1–4 workeria; 1 käyttää sarjalaskentaa |
| `/simd auto` | Käytä AVX2:ta sitä tukevilla workereilla (oletus), muuten SSE2:ta |
| `/simd sse2` | Pakota SSE2 vertailumittausta varten |
| `/selftest` | Aja laskennan tarkistus |
| `/run` | Aja ring3-koe, jonka tulos on 42 |
| `/run add 12 30` | Laske kahden luvun summa ring3:ssa |
| `/run tests` | Tarkista paluu, poikkeukset ja silmukan pysäyttäminen |
| `/run help` | Näytä prosessikokeiden ohje |
| `/asm SOURCE` | Käännä ja suorita asm1-ohjelma käsin |
| `/help` | Näytä ohje |
| `/quit` | Sammuta kone UEFI:n kautta |

## Kevyt ring3-prosessi

Kokeet voi ajaa nimillä ilman konekooditavuja: `/run`, `/run add 12 30`,
`/run div0`, `/run ud2`, `/run memory`, `/run hlt` ja `/run loop`.
`/run tests` ajaa kaikki kuusi perustarkistusta. Tulos näkyy desimaalina,
ja poikkeuksesta tulostetaan myös nimi ja virheen osoite. `/run help` näyttää
ohjeen. Edistynyt `/exec HEX` säilyy; se lisää `int 0x80` -lopetuksen tavujen
perään. Mallille tarkoitettu minimaalinen `asm1`-kääntäjä on käytettävissä
komennolla `/asm SOURCE` (rivinvaihdot voi korvata puolipisteillä). Se kääntää
rajatun kokonaislukukielen suoraan ring3-prosessiksi; kieli ja rajat ovat
[docs/asm1.md](docs/asm1.md).

Mallin järjestelmäviesti opettaa asm1-kielen syntaksin ja käyttää sitä
oletuksena assembly-/assembler-esimerkeissä. Muu murre, kuten Linux/GAS,
valitaan vain käyttäjän erillisestä pyynnöstä. Esimerkiksi `Write a simple
assembler example that adds 17 and 25.` pyytää ruudulle yhden
`/asm asm1; ...; exit rN; end` -rivin, jonka voi syöttää konsoliin kokeiltavaksi.
Tavallisiin kysymyksiin vastataan normaalisti. Vastaus tulostuu token
kerrallaan yhdellä generointikierroksella. `/asm SOURCE` suoritetaan vain
käyttäjän antamana konsolikomentona; mallin vastauksessa esiintyvä `/asm` on tavallista tekstiä.
Komennon tulosta ei lisätä mallin keskusteluun. `asm1:` näyttää komennon
tuloksen ja `AI>` mallin vastauksen.

Prosessi saa 4 KiB muuttumattoman koodisivun ja 8 KiB NX-pinon suojaussivuineen.
Kernelin muisti ei ole käyttäjätilan käytettävissä. Trap Flag rajoittaa ajon
100 000 askeleeseen; askelluksen ohittavat käskyt hylätään konservatiivisella
tavutarkistuksella. Käytössä ovat kokonaislukukäskyt, x87 ja SSE2; käyttäjäkoodin
AVX on tässä versiossa pois käytöstä. Firmware- ja liukulukutila palautetaan
ennen paluuta main loopille. Mallin SIMD-valinta toimii erikseen.
Rajapinta ja rajoitukset: [docs/process.md](docs/process.md).

`make test-process` käynnistää oikean prosessitoteutuksen QEMU/OVMF:ssä yhdellä
ja neljällä virtuaaliytimellä ilman mallin latausta. Testiin tarvitaan
`qemu-system-x86` ja `ovmf`. Tavallinen `make test` ajaa lisäksi komentotulkin
kehityskonetestit. Pelkkä käännös tai isäntäkoneen testi ei testaa ring-siirtymiä.

`Compute:`-rivi näyttää todellisen workerien määrän ja tilan: `MP pool`,
`MP blocking` tai `serial`. Vastauksen lopussa näkyy myös tokenia sekunnissa.
`Matvec:` kertoo SIMD-valinnan, BSP:n käyttövalmiuden ja viime ajossa käytetyt
käskykannat. `/selftest`-ajon jälkeen näkee myös AP:iden mahdollisen SSE2-varapolun.
Prosessorin AVX2-tuki ei yksin riitä: UEFI:n pitää sallia XMM/YMM-tila.
Ohjelma tarkistaa tämän jokaisella workerilla eikä muuta firmwaren CR4/XCR0-asetuksia.
Firmwaren sallimassa pool-tilassa BSP ja enintään kolme AP:tä laskevat
rinnakkain. Jos firmware sallii vain synkroniset MP-kutsut, BSP odottaa ja AP:t
laskevat: neljän loogisen prosessorin koneella tällöin käytössä on enintään
kolme workeria. Lisätiedot ja vertailuohje: [docs/performance.md](docs/performance.md).

Mallin tiedostomuoto on kuvattu [docs/format.md](docs/format.md), testauksen
tulokset [docs/verification.md](docs/verification.md) ja projektin synnyttäneet
keskustelut [docs/keskustelut.md](docs/keskustelut.md).

Lähdemalli on `Qwen/Qwen2.5-Coder-1.5B-Instruct`. AIOS kvantisoi sen BF16-
painot Q4-muotoon. RMSNorm-painot ja Q/K/V-projektioiden bias-vektorit ovat
FP32-muodossa. Tokenisointi käyttää NFC-normalisointia ja mallin omaa BPE-
sanastoa. Keskustelumuoto on ChatML (`<|im_start|>system`, `user`, `assistant`
ja `<|im_end|>`); AIOS-ohje annetaan system-viestinä ensimmäisellä vuorolla.
Lähde, revisio, kvantisointi ja tiiviste ovat tiedostossa `model.json`;
lisenssi on `LICENSE.Qwen` (Apache-2.0). Vanhojen mallien lisenssit säilyvät
historiaa varten.
