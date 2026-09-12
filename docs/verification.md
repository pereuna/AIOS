# UEFI-version tarkistus

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
