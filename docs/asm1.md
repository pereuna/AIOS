# asm1

`asm1` on pieni 32-bittinen kokonaislukukieli, jota käytetään komennolla
`/asm SOURCE`. Kääntäjä tuottaa rajatun x86-64-koodikatkelman ja suorittaa sen
ring3-prosessissa. Konsolissa lähderivit kirjoitetaan puolipisteillä:

```text
/asm asm1; input 12 30; ld r0 0; ld r1 1; add r0 r1; exit r0; end
```

Tämä palauttaa `42`. Mallin tuottama kutsu suoritetaan vain, jos vastauksen
ensimmäiset tavut ovat täsmälleen `/asm `, lähde päättyy erilliseen `end`-
direktiiviin ja malli päättää vastauksensa lopputokenilla. Kutsu käännetään
ja ajetaan automaattisesti. Tulos palautetaan mallille, joka jatkaa vastausta
tai korjaa ohjelmaa ilman käyttäjän toimia.

## Mallin automaattinen työkalukierros

Mallille annetaan kieliohje ja esimerkkikeskustelu (`baremetal/asm1_prompt.h`).
Järjestelmäviesti sisältää käskyt ja rajat. Onnistunut työkalukierros ja virheen
korjaus annetaan oikeina user/assistant-vuoroina lopputokeneineen, jotta malli
oppii päättämään kutsun ja odottamaan suoritustulosta. Painoja ei hienosäädetä
eikä ohjetiedostoja lueta tikulta keskustelun aikana.
Ohje ja esimerkit lasketaan kontekstiin ensimmäisellä kysymyksellä ja uudelleen
`/reset`-komennon tai kontekstin tyhjennyksen jälkeen. Tämä lisää erityisesti
ensimmäisen vastauksen odotusaikaa.

`agent_run()` sallii enintään kolme kutsuyritystä kysymystä kohti.
Käännösvirheet kuluttavat myös yrityksen. Kaikilla mallikierroksilla on
yhteinen `/tokens N` -budjetti (oletus 512); lopulliselle vastaukselle
varataan 32 tokenia. Työkalukierroksen generointi jättää kontekstiin
320 tokenin varan palautteelle ja jatkolle. Jos palautteen jälkeen mahtuu enää
lopullinen vastaus, seuraavat työkalukutsut estetään. Oletuskonteksti on 2048.

`asm1_execute()` palauttaa käännöstilan, virherivin, tiedon suoritusrajapinnan
kutsumisesta sekä prosessituloksen. Konsoli ja automaatio käyttävät samaa
rajapintaa. Luotettu `Tool result (asm1): ...` -palaute lisätään mallin
käyttämän ChatML-muodon user-viestinä ja sen jälkeen avataan uusi assistant-
vuoro. Kolmannen kutsun jälkeen mallia ohjeistetaan antamaan lopullinen
vastaus. Neljättä kutsua ei suoriteta.

Tokeni-, konteksti- tai tavurajaan katkennutta kutsua ei ajeta, vaikka
puskurissa näkyisi jo `exit` tai `end`. Myöskään virheellisiä ohjaustokeneita
sisältävää kutsua ei ajeta. Jos budjetti tai konteksti loppuu, AIOS näyttää
pysäytyksen syyn ja palaa konsoliin. `/exec` ei ole mallin automaattinen työkalu.
Ohjelman onnistunut suoritus ei yksin todista algoritmin ratkaisevan kysymystä
oikein; mallin tehtävänymmärrystä pitää arvioida erikseen.

## Lähde ja rajat

Yksi komento kirjoitetaan yhdelle riville, ja käskyt erotetaan rivinvaihdolla
tai puolipisteellä. `#` aloittaa kommentin rivin tai puolipisteen loppuun asti.
`asm1`-alkumerkintä on valinnainen. Käsin käytettäessä `end` on valinnainen;
automaattinen kutsu vaatii sen. `end`-direktiivin jälkeen sallitaan vain
tyhjää ja kommentteja. Lähteessä pitää olla `exit rN`.

Rekisterit ovat `r0`–`r9` ja tunnisteet `l0`–`l31`. Vakiot ovat desimaalisia
32-bittisiä unsigned-lukuja välillä 0–4294967295; heksalukuja tai negatiivisia
lukuja ei hyväksytä. Kovat rajat ovat:

- lähde enintään 4095 tavua kääntäjän rajapinnassa; konsolin koko komentorivi
  ja mallin koko vastaus ovat myös enintään 4095 tavua, joten `/asm ` vie siitä 5;
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
