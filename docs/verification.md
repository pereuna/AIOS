# UEFI-version tarkistus

Nykyinen suoraan USB-tikulta käynnistyvä kuva testattiin fyysisellä x86-64
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

Nykyisen kuvan eheys tarkistetaan kolmessa kohdassa: `model.json` sisältää
`model.bin`-tiedoston SHA-256-tiivisteen, pakattu käynnistyskuva sisältää mallin
CRC32:n ja `make` tulostaa valmiin `BOOTX64.EFI`-tiedoston SHA-256-tiivisteen.
