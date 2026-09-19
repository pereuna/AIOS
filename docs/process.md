# Ring3-prosessikokeet

## Käyttö

```
/run
/run add 12 30
/run div0
/run memory
/run loop
/run tests
```

`/run` palauttaa 42 ja `/run add 12 30` palauttaa 42. Yhteenlaskun syötteet
ovat 0..4294967295; ylivuoto kiertää 32 bitin mukaisesti. `div0`, `ud2`,
`memory` ja `hlt` ovat tarkoituksella virheellisiä kokeita: main loopin pitää
jatkua poikkeuksen jälkeen. `loop` pysähtyy 100 000 debug-askeleeseen.
`/run tests` odottaa näitä poikkeuksia ja tulostaa `6/6 passed`, kun ne kaikki
palautuvat oikein. `/run help` näyttää komennot.

`/exec` on raakakoodin lisärajapinta, esimerkiksi `/exec b8 2a 00 00 00`.
Se hyväksyy kokonaisia heksatavupareja ja lisää `cd 80` lopetukseksi.
Pariton tai virheellinen syöte ei käynnistä prosessia. Nimetyt kokeet eivät
tarvitse heksasyötettä. `int 0x80` on AIOS:n oma ring3-testin paluurajapinta;
se ei toteuta Linuxin systeemikutsuja. Ring3-testi ei voi suorittaa ring0:n
laiteohjausta. Nykyinen [LLM → LLVM IR → konekoodi -laskin](calc.md) käyttää
tätä ring3-harnessia myös usean kokonaislukuoperaation SSA-ohjelmien suorittamiseen.

## Rajapinta ja muistimalli

`bm_process_run(bytes, size, &result)` on synkroninen, BSP:ltä kutsuttava
rajapinta. Se sulkee mahdollisen AP-poolin, varaa tuoreet prosessisivut, ajaa
koodin, palauttaa CPU:n firmware-tilan ja vapauttaa varaukset. Ei scheduleriä,
taustaprosesseja eikä usean samanaikaisen prosessin tukea. Tila palautetaan
sekä paluuarvossa että `result.status`-kentässä, myös valmisteluvirheissä.

| Alue | Virtuaaliosoite | Oikeudet |
| --- | --- | --- |
| Koodi, 4 KiB | `0x0000400000000000` | user, read/execute |
| Suojaussivu | `0x0000400000001000` | ei mappausta |
| Pino/data, 8 KiB | `0x0000400000002000` | user, read/write, NX |
| Pinon jälkeinen sivu | `0x0000400000004000` | ei mappausta |
| Ohjelmakuvan teksti ja data | identiteettimappaus | supervisor |

Prosessin sivutaulut eivät peri firmwaren laajoja identiteettimappauksia.
Global-TLB-mappaukset tyhjennetään ennen ajoa. Kernelin tavallista pinoa,
heapia tai mallia ei mapata käyttäjälle. Ring3-poikkeukset käyttävät erillistä
TSS.RSP0-pinoa; NMI/#DF:lle on lisäksi IST-pino. Trap-polku käyttää vain
assembleria ja supervisor-sivuja. Sivutaulut eivät saa korvata olemassa olevaa
mappausta, ja käyttäjäosoitteet ovat erillään fyysisen UEFI-kuvan osoitteista.

Alussa yleisrekisterit ovat nollia; RSP osoittaa pinon loppuun miinus 16 tavua.
x87/XMM-tila alustetaan ja palautetaan FXSAVE64/FXRSTOR64:llä. Käyttäjän
`int 0x80` lopettaa ajon ja palauttaa RAX:n. Suora `ret` ei ole exit-ABI.
Koodisivun käyttämätön loppu sisältää INT3-käskyjä, joten fragmentin loppuun
valuminen antaa #BP:n. Tulos sisältää RAX:n, askeleet, vektorin, RIP:n, CS:n,
poikkeuksen error-koodin ja CR2:n (#PF:n osoite).

## Rajaukset

Tämä on kevyt, askeltava x86-64-kokeiluympäristö, ei rajoittamattoman
konekoodin tai vihamielisten ohjelmien tuotantotason sandbox. Prosessin
suorituksen ajan maskattavat laitteistokeskeytykset ovat pois käytöstä;
100 000 debug-askeleen raja ei ole seinäkelloaikainen aikaraja. REP-käskyn
iteraatiot voivat kukin tuottaa debug-askeleen. Laiteviat ja CPU:n
mikroarkkitehtuuriset sivukanavat eivät kuulu tämän eristyksen lupauksiin.

POPF/IRET voivat poistaa TF:n. MOV SS ja LSS voivat estää debug-poikkeuksen
toimittamisen. Myös XBEGIN-transaktiot suljetaan pois. Koko koodipuskuri
tarkistetaan jokaisesta tavusta (myös välittömistä vakioista), koska hyppy
voi osua keskelle käskyä. Kielletyt tavut ovat `9d`, `cf`, `8e` sekä parit
`0f b2` ja `c7 f8`. Tämä voi hylätä myös muuten kelvollisen vakion;
`/run add` rakentaa vakionsa pienistä osista tämän välttämiseksi. Koodi on
muuttumatonta ja ainoa user-executable-mappaus; pinossa rakennettua koodia
ei voi ajaa. Täysi käskydekooderi ja laitteistoajastimeen perustuva aikaraja
ovat myöhempiä laajennuksia.

Käyttäjätilan AVX/XSAVE, FSGSBASE, PKRU ja user interrupts ovat pois käytöstä
ajon aikana. SYSCALL ja SYSENTER eivät saa käyttää firmwaren mahdollisesti
asettamia ring0-kohteita. I/O-portit ja privilegiokäskyt aiheuttavat poikkeuksen.
Firmware-CR4/EFER, segmentit ja niiden kannat, debugtila, keskeytysliput sekä
x87/SSE-tila palautetaan ennen firmware-kutsuja. XCR0:a ei muuteta; AVX:n
ylemmät rekisteriosat eivät ole käyttäjäohjelman muutettavissa.

LA57-, PCID- ja CET-tilat sekä tähän toteutukseen liian suuret/epäsopivat
segmenttivalitsimet palauttavat `BM_PROCESS_UNSUPPORTED`. NX-tuki vaaditaan.
Tällöin mitään käyttäjäkoodia ei ajeta. Jos firmwarella on jo TSS, sen busy
-deskriptori palautetaan kirjoitettavan GDT-kopion kautta. Alkuperäistä GDT:tä
ei muokata. Jos firmwarella ei alun perin ole TSS:ää (TR=0), prosessin staattinen
TSS jää TR:n välimuistiin: arkkitehtuuri ei salli `LTR 0` -palautusta. Muisti
pysyy elossa koko AIOS:n ajon ajan; alkuperäiset GDTR/IDTR palautetaan silti.

## Jumituksen syy ja regressiotesti

Ensimmäisen toteutuksen poikkeuspolku jätti vektorin IRETQ-pinon alkuun;
IRETQ tulkitsi sen RIP:ksi. Lisäksi IF jäi nollaksi, TSS-rakenteesta puuttui
8 tavun varattu kenttä, NXE:tä ei varmistettu, ja vanhan busy/null-TSS:n
palautukseen käytettiin virheellistä LTR-kutsua. Näitä ei voinut havaita
pelkällä `make test` -inferenssivertailulla. Thread-määrä ei korjaa niitä.

`tests/process_uefi.c` linkitetään samoihin process/platform/MP-objekteihin
kuin varsinainen EFI-ohjelma. Erillinen EFI-testikuva ei lataa `model.bin`:iä.
Testi tarkistaa oikeat ring-siirtymät, #DE/#UD/#GP/#PF, RX/NX-suojaukset,
rikkinäisen user-pinon, I/O-kiellon, silmukan rajan, DF- ja FP-palautuksen,
firmware-rekisterit, toistuvat ajot, busy-TSS:n palautuksen sekä sivuvarausten
vapautuksen myös jokaisessa injektoidussa varausvirheessä. Jokaisen tavallisen
koeajon jälkeen firmware-ajastimen pitää edetä ja konsolin toimia. MP-laskenta
testataan ennen ja jälkeen kokeiden.

ASM-kääntäjää testataan lisäksi riippumattomia odotusarvoja vasten:
kaikki käskyt, kaikki virtuaalirekisterit, sama rekisteri molempina operandeina,
lähderekisterin säilyminen, uint32-ylivuodot, unsigned-vertailu, muistisolut,
hypyt, suurin yhteinen tekijä sekä nollalla jako ja askelraja.
Automaatiotesti syöttää ennalta määrätyt mallivastaukset ja tarkistaa koko
kierroksen käännösvirheestä ajovirheen kautta onnistuneeseen tulokseen sekä
lopulliseen vastaukseen. Varsinaiset työkalukutsut ajetaan ring3:ssa.

```
make test-process
python3 tests/process_qemu.py --accel kvm --cpus 4
```

QEMU-testin host-aikaraja on 60 s. Reset, triple fault, jumitus tai puuttuva
onnistumismerkki on epäonnistuminen. `OVMF_CODE` tai `--firmware` valitsee
firmwarekuvan; sen vierestä luetaan `OVMF_VARS_4M.fd` yksityiseen testikopioon.
Host-testit tarkistavat myös HEX-parserin vartioidulla muistialueella ja
nollavektorin (#DE) virhetulostuksen.

Arkkitehtuurin viite: [Intel 64/IA-32 SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html),
erityisesti IRET/LTR, 64-bit TSS, debug-poikkeukset ja sivusuojaus.
