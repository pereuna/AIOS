# asm1

`asm1` on pieni 32-bittinen kokonaislukukieli, jota käytetään komennolla
`/asm SOURCE`. Kääntäjä tuottaa rajatun x86-64-koodikatkelman ja suorittaa sen
ring3-prosessissa. Konsolissa lähderivit kirjoitetaan puolipisteillä:

```text
/asm asm1; input 12 30; ld r0 0; ld r1 1; add r0 r1; exit r0; end
```

Tämä palauttaa `42`. Mallin tuottama kutsu suoritetaan vain, jos vastauksen
ensimmäiset tavut ovat täsmälleen `/asm `; tavallista mallitekstiä ei tulkita
koodiksi.

## Lähde ja rajat

Yksi komento kirjoitetaan yhdelle riville, ja käskyt erotetaan rivinvaihdolla
tai puolipisteellä. `#` aloittaa kommentin rivin tai puolipisteen loppuun asti.
`asm1`-alkumerkintä ja `end` ovat valinnaisia. Lähteessä pitää olla `exit rN`.

Rekisterit ovat `r0`–`r9` ja tunnisteet `l0`–`l31`. Vakiot ovat desimaalisia
32-bittisiä unsigned-lukuja välillä 0–4294967295; heksalukuja tai negatiivisia
lukuja ei hyväksytä. Kovat rajat ovat:

- lähde enintään 4096 tavua;
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

## Esimerkki: suurin yhteinen tekijä

```text
asm1; input 48 18; ld r0 0; ld r1 1; label l0; jz r1 l1; mov r2 r0; umod r2 r1; mov r0 r1; mov r1 r2; jmp l0; label l1; exit r0; end
```

Käännösvirhe tulostetaan muodossa `compile_error E_CODE`, ja jos virhe liittyy
nimettyyn lähderiviin, mukana on myös `line N`. Onnistunut ajo tulostaa
`ok; value=N; steps=N`. Ajonaikaiset viat, kuten nollalla jako, koodin
turvatarkistuksen hylkäys ja askelraja, ilmoitetaan `runtime_error`-tuloksina.
