# asm1

`asm1` on pieni 32-bittinen kokonaislukukieli, jota käytetään komennolla
`/asm SOURCE`. Kääntäjä tuottaa rajatun x86-64-koodikatkelman ja suorittaa sen
ring3-prosessissa. Konsolissa lähderivit kirjoitetaan puolipisteillä:

```text
/asm asm1; input 12 30; ld r0 0; ld r1 1; add r0 r1; exit r0; end
```

Tämä palauttaa `42`. Komento suoritetaan vain käyttäjän syöttämänä
konsolikomentona. Mallin vastauksessa oleva `/asm` näytetään tekstinä, eikä
komennon tulosta syötetä mallin kontekstiin.

Tavallisen keskustelun järjestelmäviesti sisältää asm1-kielen ohjeen
(`ASM1_SYSTEM_PROMPT` tiedostossa `baremetal/asm1_prompt.h`). Assembler- ja
assembly-esimerkeissä käytetään oletuksena asm1:tä, ellei käyttäjä pyydä
nimenomaisesti muuta murretta. Mallia ohjeistetaan näyttämään kokonainen,
puolipistein erotettu `/asm asm1; ...; exit rN; end` -rivi. Esimerkiksi
`Write a simple assembler example that adds 17 and 25.` pyytää testattavaa
koodia. Kieli käsittelee kokonaislukuja; siinä ei ole tekstin tulostusta,
merkkijonoja tai Linux-järjestelmäkutsuja.

Ohje sisältää syntaksin, käskyt ja yhden yhteenlaskuesimerkin. Tavallisiin
kysymyksiin mallia ohjeistetaan vastaamaan normaalisti. Automaattista suoritusta, työkalupalautetta
tai korjauskierroksia ei käytetä. Sama kieliohje otetaan käyttöön myös
`/reset`-komennon ja kontekstin tyhjennyksen jälkeen. Kieliohje pidentää
ensimmäisen kysymyksen käsittelyä verrattuna pelkkään avustajan esittelyyn.
`/tokens N` rajoittaa yhtä vastausta, joka tulostuu token kerrallaan.

## Erillinen automaatiotesti

Automaation kokeellinen toteutus ja testit ovat edelleen lähdekoodissa,
mutta `agent_run()` ei ole mukana tavallisessa EFI-ohjelmassa.
`make test-agent-live` rakentaa erillisen testikuvan ja antaa mallille
nimenomaisesti `baremetal/asm1_prompt.h`-tiedoston `ASM1_AGENT_SYSTEM_PROMPT`-
ohjeen ja `ASM1_EXAMPLES`-esimerkkivuorot.
Testissä voidaan kokeilla enintään kolmen kutsun työkalukierrosta ja
virheenkorjausta. Tämä testitila ei ole tavallisen konsolin toiminto.

## Lähde ja rajat

Yksi komento kirjoitetaan yhdelle riville, ja käskyt erotetaan rivinvaihdolla
tai puolipisteellä. `#` aloittaa kommentin rivin tai puolipisteen loppuun asti.
`asm1`-alkumerkintä ja `end`-direktiivi ovat konsolikomennossa valinnaisia.
`end`-direktiivin jälkeen sallitaan vain tyhjää ja kommentteja. Lähteessä pitää olla `exit rN`.

Rekisterit ovat `r0`–`r9` ja tunnisteet `l0`–`l31`. Vakiot ovat desimaalisia
32-bittisiä unsigned-lukuja välillä 0–4294967295; heksalukuja tai negatiivisia
lukuja ei hyväksytä. Kovat rajat ovat:

- lähde enintään 4095 tavua kääntäjän rajapinnassa; konsolin koko komentorivi
  on myös enintään 4095 tavua, joten `/asm ` vie siitä 5;
- enintään 128 lähdealkiota (käskyt ja `label`-rivit yhteensä);
- enintään 64 `input`-arvoa;
- enintään 256 muistisolua;
- generoitu koodi enintään 4096 tavua.

`input` voi esiintyä kerran tai useammin. Arvot kopioidaan muistisoluihin 0–63
syöttöjärjestyksessä. Ohjelman alussa `r0` sisältää syötteiden lukumäärän ja
muut rekisterit ovat nollia. Muistisolut 0–255 ovat nollasta alustettuja
32-bittisiä soluja. `ld` ja `st` hyväksyvät vain indeksit 0–255; muu indeksi on
käännösvirhe.

## Käskyt

Kahden rekisterin käskyissä ensimmäinen rekisteri on kohde ja toinen lähde.
Kaikki arvot ovat 32-bittisiä, joten laskenta kiertää modulo 2^32.
Muut kuin kohteena olevat virtuaalirekisterit säilyvät myös jaossa ja siirroissa.

| Käsky | Merkitys |
| --- | --- |
| `li rd N` | Aseta `rd` arvoon `N` |
| `mov rd rs` | Kopioi `rs` rekisteriin `rd` |
| `add rd rs` | `rd = rd + rs` |
| `sub rd rs` | `rd = rd - rs` |
| `mul rd rs` | `rd = rd * rs` |
| `udiv rd rs` | `rd = rd / rs`, unsigned |
| `umod rd rs` | `rd = rd % rs`, unsigned |
| `and`, `or`, `xor` | Bittikohtainen operaatio kohteelle ja lähteelle |
| `shl rd rs` | Siirrä `rd`:tä vasemmalle; määrästä käytetään 5 alinta bittiä |
| `shr rd rs` | Siirrä `rd`:tä oikealle; määrästä käytetään 5 alinta bittiä |
| `eq rd rs` | Aseta `rd` arvoksi 1, jos vanhat arvot ovat samat, muuten 0 |
| `lt rd rs` | Aseta `rd` arvoksi 1, jos vanha `rd` on unsigned-pienempi kuin `rs` |
| `ld rd I` | Lue muistisolusta `I` rekisteriin `rd` |
| `st I rs` | Tallenna rekisteri `rs` muistisoluun `I` |
| `label lK` | Määrittele hyppykohde |
| `jmp lK` | Hyppää aina tunnisteeseen `lK` |
| `jz rN lK` | Hyppää, jos rekisteri `rN` on nolla |
| `exit rN` | Lopeta ja palauta rekisterin `rN` arvo |

Jakajaksi nolla aiheuttaa ajonaikaisen prosessivirheen. Liian pitkä laskenta
pysäytetään 100 000 debug-askeleen jälkeen.
Askeleet ovat generoituja konekäskyjä, eivät ASM-lähderivien lukumäärä.

## Esimerkki: suurin yhteinen tekijä

```text
asm1; input 48 18; ld r0 0; ld r1 1; label l0; jz r1 l1; mov r2 r0; umod r2 r1; mov r0 r1; mov r1 r2; jmp l0; label l1; exit r0; end
```

Käännösvirhe tulostetaan muodossa `compile_error E_CODE`, ja jos virhe liittyy
nimettyyn lähderiviin, mukana on myös `line N`. Onnistunut ajo tulostaa
`ok; value=N; steps=N`. Ajonaikaiset viat, kuten nollalla jako, koodin
turvatarkistuksen hylkäys ja askelraja, ilmoitetaan `runtime_error`-tuloksina.
`E_INCOMPLETE` tarkoittaa puuttuvaa `end`-direktiiviä automaattisessa kutsussa;
`E_ENCODING_REJECTED` tarkoittaa, että prosessin konservatiivinen tavusuodatin
hylkäsi koodauksen. Tällainen hylkäys on mahdollinen myös kelvollisessa
ohjelmassa esimerkiksi hyppysiirtymän tavujen takia. Ajonaikaisesta virheestä
ei palauteta osittaista rekisteriarvoa laskennan onnistuneena tuloksena.

`make test` tarkistaa parserin ja automaation ohjauksen kehityskoneella.
`make test-process` suorittaa käskyvertailut ja automaattisen
käännösvirhe–ajovirhe–onnistuminen–vastaus-ketjun QEMU/OVMF:ssä oikealla
ring3-toteutuksella. Näissä automaation mallivastaukset ovat ennalta määrättyjä;
ne testaavat ohjausta, eivät mallin kykyä keksiä ohjelma.

Hidas `make test-agent-live` ajaa myös nykyisen Q4-mallin ja oikean ring3-
suorituksen samassa QEMU/OVMF-koneessa kysymyksellä `What is 17+25?`.
Testi vaatii KVM:n, `mtools`-paketin ja 4 GiB muistia virtuaalikoneelle.
Se luo yksityisen 2 GiB FAT32-testikuvan, johon kopioidaan malli, ja sallii
ajolle 900 sekuntia. Tämä yksittäinen esimerkkikysymys tarkistaa integraation,
ei mallin yleistä ohjelmointikykyä. Testi ei kuulu tavalliseen `make test` -ajoon.
