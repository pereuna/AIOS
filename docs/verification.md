# UEFI-version tarkistus

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
oletuksiin testattiin. Oletuskuvan koko on 34 576 tavua ja SHA-256
`860f49c155004cae826f30b4d48b8eeb8843500dafb9067e40af1e71ca19047c`.

Uutta 1.7B-versiota ei ole vielä käynnistetty fyysisellä UEFI-koneella.
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
