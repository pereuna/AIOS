# LLM → LLVM SSA IR → x86-64 → ring3

Malli kuvaa laskun oikealla LLVM IR -syntaksilla. AIOS tarkistaa rajatun
SSA-ohjelman, muodostaa x86-64-koodin ja suorittaa sen ring3:ssa. CPU:n tulos
palaa mallin kontekstiin. Malli ei ennusta konekäskyjen heksatavuja.

Bare metal -ohjelma sisältää pienen C:llä toteutetun IR-kääntäjän
(`baremetal/ir.c`), ei LLVM-kirjastoa. Syöte on LLVM IR:n osajoukko, jonka
kelvollisuus ja määriteltyjen laskujen tulokset testataan myös oikealla LLVM:llä.
Tämä ei ole yleinen LLVM-kääntäjä eikä LLVM-bitcode-lataaja.

## Käyttö

```text
What is 1 + 100?
Multiply the sum of 1 and 100 by 3.
/calc 17 - 25
/calc (12 + 3) * (17 - 7)
```

Tavallisessa kysymyksessä laskennan tunnistus ja työkalun valinta kuuluvat
mallille. `/calc` vaatii työkalukutsun ja pyytää korjausta suorasta vastauksesta.
Kirjaimellisesta `/calc A OP B` -pyynnöstä tarkistetaan lisäksi, että IR sisältää
täsmälleen yhden vastaavan operaation alkuperäisillä int64-operandeilla ja
palauttaa juuri sen SSA-tuloksen. Laskujonojen ja sanallisten tehtävien tulkinnan
oikeellisuutta ei todisteta tällä tarkistuksella.

Mallin työkaluvastaus on kokonainen LLVM-funktio, esimerkiksi:

```llvm
define i64 @calc() {
entry:
  %sum = add nsw i64 1, 100
  %result = mul nsw i64 %sum, 3
  ret i64 %result
}
```

Koko ohjelma suoritetaan yhdellä prosessiajolla. Malli saa takaisin:

```text
Tool result (calc): ok; ir=llvm; operations=2; value=303; steps=...; executor=ring3
```

Malli vastaa alkuperäiseen kysymykseen tämän arvon perusteella. IR ja CPU:n tulos
näkyvät erikseen, joten myös valittu lasku ja mallin sanalliset virheet näkyvät.

## Tuettu LLVM IR

- Yksi `define i64 @calc()` -funktio ilman parametreja tai attribuutteja.
- Yksi peruslohko; `entry:`-otsake on valinnainen.
- Vähintään yksi `add`, `sub`, `mul` tai `sdiv` sekä yksi lopussa oleva `ret`, kaikki tyypillä `i64`.
- `add nsw`, `sub nsw` ja `mul nsw` sallitaan. Mallia ohjeistetaan käyttämään niitä.
- Etumerkilliset desimaaliset int64-vakiot ja aiemmin määritellyt nimetyt SSA-arvot.
- Saman SSA-arvon saa lukea monta kertaa, mutta määrittää vain kerran.
- LLVM:n `;`-kommentit ja välilyönnit/rivinvaihdot sallitaan.

Enintään 64 laskuoperaatiota, 128 vakion esiintymää ja 31 merkin SSA-nimet.
Nimet käyttävät ASCII-kirjaimia sekä `-`, `$`, `.`, `_` ja alun jälkeen numeroita.
Numeroidut/quoted nimet, muut lohkot, `phi`, haarat, silmukat, funktiokutsut,
muistikäskyt, muut tyypit, `nuw`, `exact`, `undef` ja `poison` hylätään.
`entry` on varattu lohkon nimeksi. Mallin koko työkaluvastaus saa olla enintään
4095 tavua, eikä mukana saa olla Markdown-aitoja tai muuta selitystä.
`ret` hyväksyy vain aiemmin lasketun SSA-arvon; suora `ret i64 101` hylätään,
jotta malli ei voi palauttaa itse laskemaansa vakiovastausta CPU:n ohi.

Tavalliset `add/sub/mul` noudattavat LLVM:n modulo 2^64 -semantiikkaa; tulos
näytetään etumerkillisenä int64-lukuna. `nsw`-ylivuoto tuottaa LLVM:ssä poison-arvon.
AIOS käyttää rajatumpaa tarkistettua ajotapaa: se pysäyttää heti poison-arvon
tuottavan operaation ja palauttaa `E_OVERFLOW`, vaikka välitulos jäisi käyttämättä.
Täyttä LLVM:n poison-semanttiikkaa ei toteuteta.

`sdiv` katkaisee kohti nollaa. Nollajako ja `INT64_MIN / -1` aiheuttavat
prosessorin jakopoikkeuksen: `E_DIVISION`, ei numeerista tulosta. Molemmat ovat
LLVM:ssä määrittelemättömiä tapauksia. Semantiikan lähde:
[LLVM Language Reference](https://llvm.org/docs/LangRef.html#binary-operations).

## Konekoodin muodostus

Parseri tarkistaa nimet, tyypit, operaatiot, syöterajat ja funktion lopetuksen.
Eteenpäin viittaukset, itsereferenssit, uudelleenmäärittelyt ja ylimääräinen
sisältö funktion jälkeen hylätään. Epäonnistunut käännös ei ole suoritettavissa.

Vakiot sijoitetaan erilliseen NX-datamuistiin 64-bittisinä arvoina, yksi solu
jokaiselle esiintymälle. SSA-välitulokset ovat datasivun kiinteissä soluissa
vakioalueen jälkeen. RAX ja RCX toimivat työrekistereinä. `ret` käännetään
tuloksen lataukseksi RAX:ään ja AIOS:n `int 0x80` -paluuksi. `nsw` tarkistaa
CPU:n ylivuotolipun ja aiheuttaa UD2:n ylivuodossa. Tämän kääntäjän UD2-poikkeus
muutetaan `E_OVERFLOW`-tulokseksi.

Luvut eivät ole konekoodin välittömiä vakioita: sama operaatio- ja viittausrakenne
tuottaa identtisen konekoodin eri syötteillä. SSA-nimien kirjoitusasu ei vaikuta
koodiin. Kääntäjä ei laske vastauksia eikä tee vakioiden laskentaa etukäteen.
Koodi muodostetaan jokaiselle kutsulle; koodivälimuistia ei vielä ole.

Nykyinen `bm_process_run_input` vastaanottaa koodin ja datan, muodostaa
eristetyn osoiteavaruuden ja suorittaa ohjelman. Koodisivu on RX, data/pino NX.
Nykyinen konservatiivinen tavusuodatin ja askelraja säilyvät. Sallitut
käskypohjat ja kohdistetut datasiirtymät läpäisevät tavusuodattimen; mielivaltaiset
syötearvot pysyvät datassa. Katso [prosessieristys](process.md).

## Palautesilmukan rajat

Vain loppuun generoitu työkaluvastaus suoritetaan. Token-, konteksti- tai
puskurirajaan katkennut ohjelma ei käynnistä prosessia. `E_FORMAT`, `E_LIMIT`
ja `E_MISMATCH` palautuvat korjattaviksi. Enintään kolme työkaluyritystä ja
neljä mallikierrosta sallitaan yhteisellä `/tokens`-budjetilla. Onnistunut
suoritus tai laskennan ylivuoto/jakopoikkeus päättää työkaluvaiheen.
Jos konteksti täyttyy tuloksen jälkeen, CPU:n tulos näkyy mutta mallin
jatkovastausta ei generoida. `/reset` tyhjentää keskustelun.

Alkuperäinen `/bytes A OP B : HEX` on säilytetty yhteensopivana erillisenä
raakakoodikokeena. Sen operandit ovat edelleen int32 ja tavut tarkistetaan
neljää täsmällistä fragmenttia vastaan. Nykyinen malliohje käyttää vain LLVM IR:ää.
`/exec` ja `/run` ovat entiset mallista riippumattomat prosessikokeet.

Linux-kehityskonsoli (`make console`) näyttää mallin IR:n tekstinä; se ei
käännä tai suorita sitä. Suoritus ja palautesilmukka ovat bare metal -konsolissa
sekä alla olevassa natiivi + QEMU -testissä.

## Testaus

```sh
make test
make test-process
make test-calc-native
```

Host-testit tarkistavat parserin, SSA-ehdot, rajat, saman koodin eri syötteillä,
kirjaimellisten pyyntöjen vastaavuuden ja mallin palautesilmukan. `test-process`
ajaa IR:stä käännetyt laskuketjut, uudelleenkäytetyt välitulokset, int64-reunat,
modulo-operaatiot, `nsw`-ylivuodot ja jakopoikkeukset oikeassa QEMU/OVMF-ring3:ssa
yhdellä ja neljällä vCPU:lla. Se tarkistaa firmware- ja rinnakkaistyöntekijätilan
palautumisen sekä prosessimuistin vapauttamisen.

Riippumaton LLVM-vertailu on valinnainen kehityskonetesti:

```sh
python3 -m venv .build/llvm-venv
.build/llvm-venv/bin/python -m pip install llvmlite==0.47.0
make test-ir-llvm LLVM_PYTHON=.build/llvm-venv/bin/python
```

LLVM tarkistaa testifunktiot ja laskee vertailutulokset. AIOS:n oma kääntäjä
suorittaa samat funktiot QEMU-ring3:ssa. Mukana on 114 määriteltyä tapausta,
myös toistettavasti generoituja laskuketjuja ja maksimikokoiset ohjelmat.
Poisonia/jakovirheitä aiheuttavat IR-testit tarkistetaan LLVM:llä syntaktisesti;
niitä ei ajeta LLVM:n natiivikoodina. Mallivastauksia ei koskaan suoriteta
kehityskoneen LLVM-JIT:ssä. LLVM/llvmlite ei ole firmware- eikä normaalin
rakentamisen riippuvuus.

`test-calc-native` käyttää oikeaa Q4-mallia ja samaa `calc_model.h`-silmukkaa
natiivisti. IR käännetään samalla C-kääntäjällä ja tavut sekä data siirretään
pieneen QEMU/OVMF-ring3-prosessiin. Testissä on neljä perusoperaatiota,
sanallinen tehtävä, `What is 1 + 100?`, kaksi laskuketjua ja tervehdys.
Se edellyttää LLVM IR -kutsuja ja CPU:n arvon mainitsevaa jatkovastausta.
Laskuketjujen on sisällettävä myös useita IR-operaatioita. Yksittäisen tapauksen
voi ajaa esimerkiksi komennolla `CALC_CASE=7 .build/test-calc-native` (indeksit 0–8).
Koe ei ole kattava luonnollisen kielen tulkinnan arvio. Malliohje sisältää
esimerkkejä, myös kaksivaiheisen summan ja kertolaskun; tulos ei mittaa
riippumatonta yleistämiskykyä. Alkuperäisellä IR-ohjeella malli teki virheitä
jakolaskun lipussa ja sulkulausekkeen tulkinnassa, joten ohjetta täsmennettiin.

Täysi `make test-calc-live` ajaa myös mallin VM:n sisällä. TCG on siihen hidas;
KVM:n saa asetuksella `CALC_ACCEL=kvm`. Aikaraja on `CALC_TIMEOUT=1800`.
Interaktiivisen konsolin saa komennolla `make qemu` tai `make qemu QEMU_ACCEL=kvm`.
Älä aja natiivitestiä samaan aikaan muiden saman työtilan muokkaavien make-ajojen kanssa.
