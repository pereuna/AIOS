> Historiallinen testiloki. ASM1-kokeilu ja sen työkalut on poistettu 18.9.2026.
> Nykyinen kokeilu: [LLM → LLVM SSA IR → x86-64 → ring3](calc.md).

## LLM → LLVM SSA IR → x86-64 → ring3 (19.9.2026)

Mallin toistama `/bytes`-konekoodi korvattiin rajatulla, oikeaa LLVM IR
-syntaksia käyttävällä SSA-ohjelmalla. Bare metal -ympäristöön lisättiin pieni
C-kääntäjä operaatioille `add`, `sub`, `mul` ja `sdiv`. Malli voi ketjuttaa
operaatioita ja käyttää aiempia SSA-tuloksia; AIOS sijoittaa vakiot ja
välitulokset NX-datamuistiin, muodostaa x86-64-koodin ja suorittaa koko ohjelman
yhdellä ring3-ajolla. LLVM ei ole firmware-riippuvuus.

Host-testit tarkistivat tiukan parserin, SSA-nimien määrittelyn ja käytön,
64 operaation ja 128 vakion rajat, katkenneiden ohjelmien hylkäyksen,
kirjaimellisen `/calc A OP B` -pyynnön vastaavuuden sekä palautesilmukan.
AddressSanitizer- ja UndefinedBehaviorSanitizer-ajot läpäisivät IR-kääntäjän,
laskureitityksen ja mallin ohjaussilmukan testit.

`make test-process` läpäisi QEMU/OVMF:ssä yhdellä ja neljällä vCPU:lla. Mukana
olivat SSA-ketjut, välituloksen uudelleenkäyttö, int64-reunat, modulo 2^64
-laskenta, `nsw`-ylivuodot, nollajako ja `INT64_MIN / -1`. Jokaisen tapauksen
jälkeen tarkistettiin prosessimuistin vapautuminen sekä firmware- ja
rinnakkaistyöntekijätilan palautuminen.

Erillinen LLVM 20.1.8 -vertailu hyväksyi testien IR:n ja laski 114 määritellyn
tapauksen vertailuarvot. AIOS:n C-kääntäjän tuottama koodi antoi samat arvot
oikeassa QEMU-ring3:ssa. Ylivuotoa tai jakovirhettä aiheuttavaa määrittelemätöntä
LLVM-koodia ei ajettu LLVM-JIT:ssä; sen syntaksi tarkistettiin ja AIOS:n
määritelty virhepolku testattiin erikseen.

Oikea Q4-malli tuotti muun muassa kysymykseen `What is 1 + 100?` yhden
`add nsw i64` -operaation ja sai CPU:lta arvon 101. Kaksivaiheiseen kysymykseen
se tuotti `add` → `mul` -ketjun, jonka ring3-tulos oli 303. Sulkulausekkeeseen
se tuotti erilliset `add`- ja `sub`-välitulokset sekä niitä käyttävän `mul`-
operaation; ring3-tulos oli 150. Mallipromptia täsmennettiin oikean LLVM
`sdiv`-syntaksin ja sulkujen säilyttämisen osalta havaittujen virheiden jälkeen.
Koko yhdeksän tapauksen `test-calc-native`-ajo läpäisi myös neljä perusoperaatiota,
sanallisen kertolaskun ja laskemattoman tervehdysreitin.

Lopullinen EFI-kuva rakennettiin viimeisen parseritiukennuksen jälkeen. Sen koko
on 165 689 tavua ja SHA-256 on
`bdd8d51fe014ce32772c72b92feb79d982b104630808cdef8550908ead9c7724`.
Käyttäjä varmisti edeltävän toiminnallisesti vastaavan EFI-kuvan toiminnan
fyysisellä raudalla. Tämä on manuaalinen käynnistys- ja käyttösavutesti;
laitteiston tarkkaa kokoonpanoa tai erillistä automaattista testiraporttia ei
tallennettu.

## LLM → konekoodi → ring3 -laskin (18.9.2026)

ASM1- ja GNU as -oletusten tilalle lisättiin suora `/bytes`-protokolla.
`model.bin` rakennettiin lukituista Qwen2.5-Coder-1.5B-Instruct-lähdepainoista;
SHA-256 vastasi projektin `model.json`:n arvoa. UEFI-konsoli ja `dist/model.000`
rakentuivat. Mallipainoja tai tokenizeria ei hienosäädetty.

`make test PYTHON=.training-venv/bin/python` läpäisi 39 Python-testiä,
C-testit ja C/NumPy-laskentavertailun. Laskimen host-testit varmensivat myös
rajatut kutsut, katkenneiden/ristiriitaisten tavujen hylkäämisen, palautteen,
nollajaon ohjauspolun ja pitkien tavallisten tekstivastausten tulostuksen.
`make test-process` läpäisi yhdellä ja neljällä vCPU:lla myös neljä laskuoperaatiota,
int32-reunat, 64-bittiset tulokset, nollajaon, muistivarauksien vapautuksen ja
CPU-/firmware-/MP-tilan palautuksen.

Oikean Q4-mallin koe käytti `test-calc-native`-polkua: sama `calc_model.h`-
ohjaussilmukka ja `neural.c` ajoivat mallin natiivisti, ja alkuperäiset tavut
sekä operandit siirrettiin QEMU/OVMF-ring3-prosessiin. Kaikki kuusi tapausta
läpäisivät: 12 + 30 → 42; 17 - 25 → -8; 6 * 7 → 42; -17 / 5 → -3;
13 laatikkoa, 9 pulttia kussakin → 117; tervehdys → ei työkalukutsua.
Jokaisen laskun jälkeen malli sai CPU:n arvon ja mainitsi sen jatkovastauksessa.
Sanallisesta tehtävästä malli tuotti itse `/bytes 13 * 9 : ...`; konsoli ei
poiminut operandia/operaatiota kysymyksestä sääntöparserilla.

Täysi, mallinkin VM:n sisällä ajava TCG-koe pysäytettiin eikä sitä merkitä
läpäistyksi. KVM ei ole tässä ympäristössä käytettävissä. Natiivi + QEMU-ring3
-koe varmentaa koko laskuketjun, mutta firmwareympäristössä ajettavan mallin
livevarmennus jää erilliseksi `make test-calc-live` -kokeeksi. Tämä pieni koe
osoittaa protokollan toiminnan; se ei ole kattava kieli- tai laskutaitojen arvio.

# UEFI-version tarkistus

## Qwen2.5-Coder-1.5B-Instruct (13.9.2026)

Seed-Coder-8B korvattiin mallilla `Qwen/Qwen2.5-Coder-1.5B-Instruct`,
revisio `2e1fd397ee46e1388853d2af2c993145b0f1098a`. BF16-lähdepainojen
SHA-256 tarkistettiin ennen muunnosta. Q4-malli on 872 253 632 tavua
(noin 831,85 MiB), SHA-256
`6f8e43973023109fe0396273c475eeb19058fbb986d816c650ed4a290f05f96e`.
Painot ja 2 048 tokenin FP32-KV-välimuisti vievät yhteensä noin 944 MiB.

Toteutus käyttää Qwenin 28 kerrosta, 12 Q-päätä ja kahta KV-päätä sekä
jaettuja embedding-/ulostulopainoja. Q/K/V-bias-vektorit säilytetään FP32-
muodossa ja lisätään ennen RoPEa. ChatML-viestit käyttävät Qwenin oikeita
aloitus- ja lopetustokeneita; pienet token-ID:t 0, 1 ja 2 ovat tavallista
tekstiä. NFC, lisätyt koodi-/työkalutokenit ja rivinvaihtojen käsittely
vastaavat alkuperäistä tokenisointia.

`make test` läpäisi: viisi Python-testiä (45 tokenisoinnin ja keskustelumuodon
vertailutapausta), tiedostonlataajan 15 tapausta, MP/SIMD-, komentotulkki-
ja agenttisovittimen testit sekä riippumaton NumPy-päättely. Suoraan
Hugging Facen `tokenizers`-toteutukseen verrattiin yhteensä 205 tekstitapausta;
kaikki vastasivat. C:n 607 744 logitin suurin absoluuttinen ero NumPyyn oli
0,00019741. SSE2/AVX2, eri ydinmäärät ja dispatch-tilat tuottivat keskenään
bittitasolla samat tulokset.

`make test-model-uefi` läpäisi QEMU/OVMF:ssä KVM:llä, neljällä vCPU:lla ja
2 GiB:n muistilla. Mallin lataus ja CRC32 onnistuivat. Syötteen
`[151644, 872, 198, 3838]` greedy-tulokset olivat sekä kehityskoneella että
UEFI:ssä `[17, 40, 198, 358]`.

Todellinen keskustelukoe käytti AIOSin system-ohjetta ja kysymystä
`What is 2+2? Reply with only the number.` Malli vastasi `4` ja päätti vuoron.
Syötteessä oli 427 tokenia; koko koe kesti tällä kehityskoneella 149,75 sekuntia
neljällä workerilla ja AVX2-laskennalla.

USB-jakelu sisältää `dist/EFI/BOOT/BOOTX64.EFI` ja yhden `dist/model.000`-
tiedoston. Vanhan jakelun `model.001` ja `model.002` poistettiin paikallisesta
`dist`-hakemistosta. `dist/model.000`-tiedoston SHA-256 tarkistettiin mallin
tiivistettä vasten. EFI-kuvan SHA-256 on
`91e7c5b32f33c8c86dbdbbd4c42463eddd35e012590c70f1364cf774ea9d99c6`.
Fyysiselle USB-tikulle ei tehty asennusta tämän vaihdon yhteydessä.

Alla olevat Seed-Coder- ja SmolLM2-mittaukset ovat aiempien malliversioiden
historiaa.

## Seed-Coder-8B-Reasoning-bf16 (13.9.2026)

Oletusmalli vaihdettiin lähteeseen `ByteDance-Seed/Seed-Coder-8B-Reasoning-bf16`,
revisio `aa14634327ec15c9b9130243104cb2df400f1a60`. Kaikkien neljän BF16-
painotiedoston SHA-256 tarkistettiin. AIOS käyttää niistä muodostettua Q4-
mallia: 4 645 267 520 tavua, SHA-256
`24f895d8e4acf559caf642ae6b44a4a3e673f4fc76ab417fdf0291b9cc07174a`.

`make test` läpäisi: viisi Python-testiä (mukaan lukien 35 upstream-
tokenisoinnin vertailutapausta), lataajan 15 tapausta, MP/SIMD-, komentotulkki-
ja agenttisovittimen testit sekä riippumaton NumPy-päättely. Kaikki 620 544
logitia vastasivat NumPy-vertailua; suurin absoluuttinen ero oli 0,00007343.
SSE2/AVX2-, ydinmäärä- ja dispatch-vaihtoehdot tuottivat keskenään bittitasolla
samat logitit. Syötteen `[0, 4169, 326, 3857]` greedy-tulokset olivat
`[326, 326, 130, 684]`.

Lisäksi 162 tekstitapausta verrattiin suoraan Hugging Facen `tokenizers`-
toteutukseen. Vertailussa olivat muun muassa koodi, rivinvaihdot, isot kirjaimet,
luvut, Unicode ja NFC-normalisointi. AIOS-ohjeen sisältävä ensimmäinen
käyttäjävuoro vastasi myös alkuperäisen keskustelumallin tokenisointia.

USB-jakelu on `dist/EFI/BOOT/BOOTX64.EFI` sekä `dist/model.000`–`model.002`.
Osien yhdistetty SHA-256 tarkistettiin mallin tiivistettä vasten. EFI-kuvan SHA-256
on `3dfe377e5754c91d026d4019fbccd69a5be6cc9a81e0270d486de89fb7b32196`.
`tests/model_uefi.c` ajettiin QEMU/OVMF:ssä KVM:llä, neljällä vCPU:lla ja
8 GiB:n muistilla: kolmen FAT32-osan lataus, CRC32 ja neljän tokenin päättely
läpäisivät, ja greedy-tulokset olivat samat kuin kehityskoneella.
Testin voi toistaa komennolla `make test-model-uefi`.

Fyysiselle USB-tikulle ei tehty tämän vaihdon yhteydessä asennusta eikä
uudelle mallille ajettu fyysisen koneen käynnistystestiä. Alla olevat vanhemmat
mittaukset ja keskustelukokeet koskevat aiempia SmolLM2-versioita.

## Asm1-koodiesimerkit tavallisessa keskustelussa (13.9.2026)

Tavalliseen järjestelmäviestiin lisättiin `asm1_prompt.h`-tiedoston
`ASM1_SYSTEM_PROMPT`: kielikuvaus, käskyt ja yksi yhteenlaskuesimerkki.
Assembly-esimerkkien oletus on asm1. Malli näyttää käsin ajettavan
`/asm ...` -rivin; automaattinen suoritus ja työkalupalautekierros pysyvät
poissa normaalista EFI-ohjelmasta. Automaatiotestin vanha ohje on erillinen
`ASM1_AGENT_SYSTEM_PROMPT`.

`make test` läpäisi. Todellinen Q4-malli sai pyynnön `Write a simple assembler
example that adds 17 and 25.` ja tuotti selityksen yhteydessä rivin:

```text
/asm asm1; li r0 17; li r1 25; add r0 r1; exit r0; end
```

Rivi läpäisi varsinaisen asm1-kääntäjän: 21 tavua konekoodia ja kelvollinen
`end`-direktiivi. Tämä mallikoe tarkisti tuotetun koodin kääntymisen;
generoitua ohjelmaa ei ajettu automaattisesti. Yksi onnistunut esimerkki ei
takaa kaikkien mallin tuottamien ohjelmien oikeellisuutta.

Koepyynnön koko syöte oli 469 tokenia. Kysymyksellä `What is 17+25?` syöte
on nyt 462 tokenia, kun pelkällä esittelyviestillä se oli 33 ja vanhalla
automaatiolla 671. Todellisella tokenizerilla tarkistettiin kieliohjeen
mukanaolo ensimmäisessä vuorossa ja nollauksen jälkeen sekä ohjeeton
jatkovuoron kehystys. Kieliohje kasvattaa ensimmäisen vuoron laskentatyötä;
fyysisen koneen vastenopeutta ei mitattu.

EFI-kuva sisältää uuden kieliohjeen ja `/asm`-konsolikomennon, mutta ei
`agent_run()`-symbolia tai automaation työkalupalauteohjetta. Kuvan SHA-256:
`91cee3bbec3f1ec301b9271ffc7993fb45aea47381bc4adf506bd19c307280e7`.

## Tavallinen keskustelu ilman autonomista asm1:tä (12.9.2026)

Normaalista järjestelmäviestistä poistettiin asm1-ohje ja esimerkkivuorot.
EFI-konsoli generoi yhden vastauksen token kerrallaan ilman automaattista
työkalukierrosta tai sen kontekstivarausta. Käyttäjän `/asm SOURCE` säilyy
erillisenä konsolikomentona. Kokeellinen agentti on vain erillisissä testikuvissa.

`make test` ja `make test-process` läpäisivät; jälkimmäinen tarkisti asm1:n,
ring3-ajot ja konsolin QEMU/OVMF:ssä yhdellä ja neljällä virtuaaliytimellä.
Kysymyksen `What is 17+25?` ensimmäinen syöte lyheni todellisella tokenizerilla
671 tokenista 33 tokeniin. Jatkovuoron kehystys ja nollauksen jälkeinen
alkuperäisen lyhyen syötteen palautuminen tarkistettiin myös. Oikean Q4-mallin
host-kokeen vastaus oli `17 + 25 = 42.`.

Normaalin EFI-kuvan symbolitaulussa ei ole `agent_run()`-funktiota, eikä kuva
sisällä asm1:n järjestelmäohjetta tai työkalupalautteen ohjeita. Erillinen
`agent-live.efi` rakentui; sen hidasta malliajoa ei toistettu tässä muutoksessa.
Fyysisen koneen vastenopeutta ei mitattu. Uuden `BOOTX64.EFI`-kuvan SHA-256 on
`5e7bd9bea0b6549f9ad1e6195a8e84ad1fa035b0a62cc26db0269283fbd7c19a`.

## ASM-kääntäjä ja automaattinen työkalukierros (12.9.2026)

Kääntäjän virtuaalirekisterit erotettiin pinosta ja apurekistereistä. Korjaukset
kattavat `exit rN` -paluun, jako- ja jakojäännöskäskyt, bittisiirrot,
vertailuliput sekä lähderekisterien säilymisen. Parseri tarkistaa lähteen ja
konekoodin kokorajat, pitkät sanat, virherivit sekä `end`-direktiivin.

`make test-process` läpäisi QEMU/OVMF:n TCG-ajoissa yhdellä ja neljällä
virtuaaliytimellä. ASM-kokeissa verrataan käskyjen tuloksia riippumattomiin
odotusarvoihin kaikilla virtuaalirekistereillä, myös samalla kohde- ja
lähderekisterillä. Mukana ovat ylivuodot, unsigned-vertailut, muistisolut,
hypyt, suurin yhteinen tekijä, nollalla jako ja käskybudjetin loppuminen.
Automaation integraatiotesti käy ennalta määrätyillä mallivastauksilla läpi
käännösvirheen, ring3:ssa syntyvän ajovirheen, korjatun laskun ja lopullisen
vastauksen. Firmware-tila, muistivarausten vapautus ja MP-poolin toiminta
tarkistetaan myös työkalupalautteiden välissä.

`make test` läpäisi. Uudet host-testit kattavat korjausyritykset, kolmen kutsun
rajan, yhteisen tokenibudjetin, osiin jakautuvan `/asm `-alkumerkinnän,
lopputokenin, katkenneet ja liian pitkät vastaukset, ohjaustokenit sekä
palautteen viestikehystyksen ja kontekstitilan riittävyyden. Parseri- ja
automaatiotestit läpäisivät myös AddressSanitizerin ja UndefinedBehaviorSanitizerin.
C/NumPy-vertailun suurin logittiero säilyi arvossa 0,00023842; testatut
SSE2/AVX2- ja MP-vaihtoehdot olivat bittitasolla samoja.

Myös hidas mallin integraatiotesti läpäisi QEMU/OVMF:ssä KVM:llä, neljällä
virtuaaliytimellä ja 4 GiB RAMilla. Käytössä oli oikea Q4-malli, sama
generointi-/palautekoodi kuin sovelluksessa ja oikea ring3-prosessi.
Kysymykseen `What is 17+25?` malli tuotti:

```text
/asm asm1; li r0 17; li r1 25; add r0 r1; exit r0; end
```

Prosessi palautti `ok; value=42; steps=5`. AIOS lisäsi tuloksen mallin
kontekstiin, minkä jälkeen malli vastasi `17+25 = 42.` ja päätti vuoronsa.
Käyttäjän välisyötettä ei tarvittu. Testin voi toistaa komennolla
`make test-agent-live`. Tämä vahvistaa yhden kokonaisen käyttötapauksen;
laaja algoritmien osaamistesti ja oikean mallin korjausyritysten onnistumisaste
ovat vielä mittaamatta. Korjausohjauksen virhepolut testattiin erikseen
ennalta määrätyillä mallivastauksilla.

Oletusasetukset ovat nyt `CONTEXT=2048 TOKENS=512`. Mallipainojen SHA-256
säilyi ennallaan. Uusi ohjeistus ja esimerkkikeskustelu tulevat EFI-ohjelmasta;
painoja ei hienosäädetty. Tämän version fyysisen koneen rautatesti on vielä
tekemättä.

## Ring3-palautuksen korjaus (12.9.2026)

Käyttäjän raportoima `/run`-jumitus toistettiin vanhalla prosessikoodilla
QEMU/OVMF:ssä: testi tulosti aloituksen, mutta ei palannut onnistuneesti.
Korjatussa koodissa IRETQ-pino, IF-lipun palautus, TSS:n rakenne ja
busy/null-TR:n käsittely sekä NX/CPU-tilan valmistelu on korjattu.
Käyttöliittymään lisättiin nimetyt `/run`-kokeet ja desimaalinen tulos.

`make test-process` läpäisi oikeat ring3-kokeet QEMU TCG:llä yhdellä ja
neljällä virtuaaliytimellä. KVM-ajot läpäisivät myös yhdellä ja neljällä ytimellä;
lopullinen neljän ytimen ajo sisälsi varausvirheiden injektoinnin. Käytössä
oli QEMU 11.1.1 ja OVMF 2026.05. Kokeet kattavat paluun, poikkeukset,
suojaukset, käskybudjetin, toistot, CPU/FP-tilan palautuksen, firmware-ajastimen,
MP-rivitehtävät ja komentorajapinnan. Testin yksityiskohdat ja rajoitukset ovat
tiedostossa [process.md](process.md).

Host-komentotulkin testit läpäisivät myös AddressSanitizerin ja
UndefinedBehaviorSanitizerin. `make test` läpäisi: EFI/Unicode-testit,
tiedostonlataaja, MP, SIMD, uusi komentotulkki ja C/NumPy-vertailu.
196 608 logitin suurin C/NumPy-ero oli 0,00023842; tokenit `[805, 198, 2, 17]`.
SSE2/AVX2 ja worker-/dispatch-tilat säilyivät bittitasolla samoina.

Korjatun EFI-kuvan koko on 82 080 tavua ja SHA-256
`1b0cfe72b2a514861e71b437437f8bd5237bb36c19c12f1ccb4e84dde12208d9`.
Fyysisen koneen rautatesti tälle kuvalle on vielä tekemättä. USB-tikkua
ei ollut liitettynä korjauksen valmistuessa, joten kuvaa ei siirretty tikulle.

## AVX2-version kehityskonetestit (11.9.2026)

`make test` läpäisi. Kaikki 196 608 logittia täsmäsivät bittitasolla SSE2:n
ja AVX2:n välillä yhdellä, kahdella ja neljällä workerilla sekä synkronisessa
AP-tilassa. Automaattinen valinta käytti kehityskoneella AVX2:ta.
Riippumattoman NumPy-vertailun suurin ero oli 0,00023842 ja tokenit
`[805, 198, 2, 17]`.

Ennen AVX2-muutosta käännetty testiohjelma, uusi pakotettu SSE2 ja uusi AVX2
tuottivat saman logittitulosteen SHA-256-tiivisteen:
`844dd2320299ff683e09a1964c55e8f27f78dfe205ad1d989c1daa028f983571`.

SIMD-ytimien testit kattoivat kaikki 65 536 FP16-bittikuviota, riippumattoman
skalaarisen matvec-vertailun, suojaussivut, kohdistamattomat syötteet,
rivialueet ja workerikohtaisen AVX2-tuen puuttumisen. Samat testit läpäisivät
AddressSanitizerin, UndefinedBehaviorSanitizerin ja ThreadSanitizerin.

Valmiin EFI-kuvan konekoodissa AVX-käskyjä esiintyi vain
`matvec_rows_avx2()`-funktiossa, eikä FMA-käskyjä ollut. Linkitettyyn kuvaan
ei jäänyt ratkaisemattomia symboleja. EFI-kuvan koko on 41 866 tavua ja SHA-256
`b2547e66837e49757f23bb53d735cf634d3236f455ab1ade04b744a93b2e7dee`.
Malli rakennettiin uudelleen ja sen SHA-256 säilyi arvossa
`a309d378bc03490e65f8e88a76ee2482f85751a248731ec3663b1647447b7620`.
AVX2-versio tarvitsee vielä oman UEFI-rautatestinsä.

`make bench` mittasi kahden workerin AVX2-ytimelle 3,48 tokenia/s, noin 3,05×
saman worker-määrän SSE2-tuloksen. Neljällä workerilla AVX2 saavutti
3,23 tokenia/s. Mittaustapa ja kaikki mediaanit ovat
tiedostossa [performance.md](performance.md#avx2-vertailu-1192026).

## Vahvistettu rautakäynnistys (11.9.2026)

Käyttäjä vahvisti tämän keskustelun rautatestissä järjestelmätyökaluilla
rakennetun yhden ytimen SmolLM2-1.7B-version toimivan bare metal -tilassa.
Tikulle kirjoitetun EFI-kuvan SHA-256 oli
`209f64a37d84a820fe2e7f5566f7cc8400cac1959c2dc4ec6a61cbd76d77bbb4`, ja mallin
tiiviste vastasi `model.json`-tiedostoa. Tämä vahvistus ei vielä koske
sen jälkeen lisättyä MP-worker poolia. Sen testit ja rautavertailun ohjeet ovat
tiedostossa [performance.md](performance.md).

## MP-rinnakkaisversion kehityskonetestit (11.9.2026)

`make test` läpäisi: kaksi Python-rakennustestiä, tiedostonlukijan 13 tapausta
ja CRC32-tarkistus, MP-rivijaon/elinkaaren/virhepolkujen testit sekä
C/NumPy-vertailu. Kahden ja neljän workerin pooli sekä synkroninen AP-polku
tuottivat kaikki 196 608 logittia bittitasolla samoina kuin sarjapolku.
C/NumPy-vertailun suurin ero oli 0,00023842 ja tokenit `[805, 198, 2, 17]`.

Uutta sarjapolkua verrattiin lisäksi ennen MP-muutosta käännettyyn
testiohjelmaan: molempien logittitulosteen SHA-256 oli
`844dd2320299ff683e09a1964c55e8f27f78dfe205ad1d989c1daa028f983571`.
MP-testit läpäisivät erikseen AddressSanitizerin, UndefinedBehaviorSanitizerin
ja ThreadSanitizerin tarkistukset. Nämä käyttävät simuloitua firmwarea;
oikeaa UEFI-aikataulutusta tai laitteistovirheitä ne eivät todista toimiviksi.

`make bench` mittasi neljän workerin poolille 2,42× nopeutuksen tämän koneen
sarjapolkuun nähden. Mittaustapa ja kaikki tulokset ovat
tiedostossa [performance.md](performance.md#mitattu-tulos-1192026).

Uusi EFI-kuva on 39 858 tavua ja sen SHA-256 on
`8ffe20b5192f8a5bca438707a0305cf30728974d0bc7cecc12ac06d3ac177455`.
Linkitetyssä kuvassa ei ole ratkaisemattomia symboleja. Malli ja sen tiiviste
ovat muuttumattomat. Tämä SSE2/MP-kuva kopioitiin myöhemmin samana päivänä
muistitikulle ja kopion tiiviste tarkistettiin. MP-version rautatestin tulosta
ei ole vielä vahvistettu.

## Rakennus järjestelmätyökaluilla (11.9.2026)

Tavallinen `make test` läpäisi järjestelmän Python 3.14.7:llä, NumPy 2.4.6:lla
ja GNU binutils 2.47:llä ilman venv-ympäristöä tai pip-asennuksia. Malli
muunnettiin uudelleen lähdepainoista: koko säilyi 964 120 960 tavuna ja SHA-256
täsmäsi `model.json`-tiedoston arvoon
`a309d378bc03490e65f8e88a76ee2482f85751a248731ec3663b1647447b7620`.

Pythonin oma Unicode-versio oli 16.0.0. Exportteri käytti repositoryyn
tallennettua Unicode 15.1 -taulukkoa, jonka kaikki 807 aluetta verrattiin
ennen muutosta vanhan exportterin tulokseen ja alkuperäiseen malliin.
Uusi regressiotesti tarkistaa taulukon binääriesityksen tiivisteen.

EFI-kuva rakennettiin Makefilen komennolla `objcopy -O pei-x86-64 --subsystem=10`
säilyttäen tarvittavat osiot. Toinen uusi testi tarkistaa AMD64-kohteen,
PE32+-otsakkeen, EFI Application -alijärjestelmän ja relokaatiotaulukon.
Kuvan koko on 35 883 tavua ja SHA-256
`209f64a37d84a820fe2e7f5566f7cc8400cac1959c2dc4ec6a61cbd76d77bbb4`.

Molemmat uudet testit, tiedostonlukijan 13 tapausta ja CRC32-tarkistus
läpäisivät. C/NumPy-vertailun 196 608 logitin suurin absoluuttinen ero oli
0,00023842, ja ahneesti valitut tokenit täsmäsivät: `[805, 198, 2, 17]`.
Tätä kehityskoneen testiä seurasi yllä kuvattu käyttäjän vahvistama rautakäynnistys.

## SmolLM2-1.7B ja erillinen mallitiedosto (11.9.2026)

`make test` rakentaa EFI-ohjelman ja mallin sekä ajaa kehityskoneella:

- todellisen UEFI-tiedostonlukijan 13 testiä simuloiduilla firmware-palveluilla:
  oikea tiedosto, lyhyet lukutulokset, puuttuvat palvelut/tiedosto, virheelliset
  tiedostotiedot/koko, hakemisto tiedoston sijaan, muistin loppuminen,
  lukuvirhe, ennenaikainen EOF, liian suuri lukutulos ja sulkemisvirhe;
- CRC32:n tunnetun tarkistusvektorin;
- saman C-transformerin ja SSE2-matematiikan vertailun riippumattomaan NumPy-
  toteutukseen samoilla Q4-painoilla. NumPy laskee neljä tokenia rinnakkain
  kausaalimaskilla; C käyttää varsinaista tokenikohtaista KV-välimuistiaan.

Neljästä tokenista tarkistettiin kaikki 196 608 logittia. Suurin absoluuttinen
ero oli 0,00024366, ja ahneesti valitut tokenit täsmäsivät: `[805, 198, 2, 17]`.
Tämä tarkistaa laskennan, ei kvantisoinnin laatua suhteessa alkuperäisiin
BF16-painoihin. Aiemman mallin perpleksisyysluvut eivät koske tätä mallia.

Lähdepainojen SHA-256 tarkistettiin. Kaksi Q4-muunnosta tuotti saman 964 120 960
tavun tiedoston ja `model.json`-tiivisteen. Tiedostonlukijan testit läpäisivät
myös AddressSanitizer- ja UndefinedBehaviorSanitizer-tarkistukset.
Kehityskoneella tehty keskustelutesti tuotti 24 tokenia ilman laskentavirheitä;
tämä ei takaa vastauksen asiatietojen oikeellisuutta.

Kontekstin vaihtaminen komennolla `make CONTEXT=2048 TOKENS=64` ja palautus
oletuksiin testattiin. Aiemman työkaluketjun oletuskuvan koko oli 34 576 tavua ja SHA-256
`860f49c155004cae826f30b4d48b8eeb8843500dafb9067e40af1e71ca19047c`.

Tämän alkuperäisen testiraportin aikaan 1.7B-versiota ei ollut vielä
käynnistetty fyysisellä UEFI-koneella; myöhempi vahvistus on yllä.
Rautatesti: kopioi molemmat `dist`-hakemiston osat tikulle, tarkista latauksen
ja CRC32:n onnistuminen, kokeile keskustelua, `/selftest`, `/reset` ja `/quit`.

## Aiempi SmolLM2-135M-versio (10.9.2026)

Alla olevat rautatestit ja laatuluvut koskevat aiempaa 135M-mallia, eivät
nykyistä 1.7B-versiota tai sen erillistä UEFI-tiedostonlatausta.

Aiempi suoraan USB-tikulta käynnistyvä kuva testattiin fyysisellä x86-64
UEFI-koneella 10.9.2026. Käynnistys, USB-näppäimistö, mallin CRC32-tarkistus,
keskustelu ja UEFI-sammutus toimivat ilman käyttöjärjestelmää ja verkkoa.

Ensimmäinen rautaversio pysähtyi kysymykseen `What is horse` virheellä
`non-finite logits`. Vika saatiin toistettua jättämällä x87:n kanssa yhteinen
MMX-rekisteripino aktiiviseksi ennen RoPE-taulun muodostamista. Vanha
x87-matematiikka tuotti NaN-arvoja.

Korjattu versio:

- käyttää vain SSE2:ta ja käännetään ilman x87- ja MMX-käskyjä;
- alustaa MXCSR:n arvoon `0x1f80` ennen itsenäisiä laskentavaiheita;
- laskee sinin, kosinin ja positiivisen potenssin SSE2-yhteensopivilla
  double-polynomeilla;
- tarkistaa käynnistyksen matematiikan, RoPE-taulun, jokaisen kerroksen tuloksen
  ja logitit NaN- ja äärettömien arvojen varalta.

Korjattu tiedosto käynnistyi samalla fyysisellä koneella ja keskustelu toimi.
Matematiikan testissä sinin, kosinin ja eksponentin suurin ero
standardikirjastoon oli yksi ULP; neliöjuuri ja testatut potenssit täsmäsivät.
Kysymys `What is horse` tuotti kelvolliset logitit myös sotketun MMX/MXCSR-tilan
jälkeen.

Mallin Q4-inferenssi oli ennen UEFI-rajausta verrattu erilliseen Transformers-
toteutukseen. C-toteutuksen suurin logittiero samoilla kvantisoiduilla painoilla
oli 0,000116, ja 24 tokenin ahne vastaus täsmäsi. Pienen 136 tokenin näytteen
perpleksisyys oli BF16-painoilla 22,20 ja Q4-painoilla 31,07; tämä mittaa
kvantisoinnin laatuhävikkiä pienellä näytteellä, ei yleistä mallilaatua.

Kuvan eheys tarkistetaan kolmessa kohdassa: `model.json` sisältää
`model.bin`-tiedoston SHA-256-tiivisteen, EFI-ohjelma sisältää mallin
CRC32:n ja `make` tulostaa valmiin `BOOTX64.EFI`-tiedoston SHA-256-tiivisteen.
