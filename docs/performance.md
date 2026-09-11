# Matvec-rinnakkaisuus

`matvec()` jakaa matriisin tulosrivit enintään neljään peräkkäiseen alueeseen.
Kukin worker lukee samoja syötevektoreita ja kirjoittaa vain omat tulosrivinsä.
Yhden rivin SSE2-laskenta ja summausjärjestys ovat samat kuin sarjaversiossa.
Malli, kvantisointi ja kontekstimuistin koko eivät muutu.

## UEFI-toteutus

`baremetal/mp.c` etsii PI MP Services -protokollan ja valitsee terveistä,
käytössä olevista prosessoreista ensin eri fyysiset ytimet, sitten SMT-sisarukset.
Prosessoreita ei kytketä firmwarelta päälle tai pois.

- `MP pool`: BSP ja enintään kolme AP:tä laskevat rinnakkain. AP:t käynnistetään
  kerran vastauksen tai selftestin ajaksi. Jokainen matvec välittää uuden työn
  release/acquire-atomisilla komennoilla ja odottaa kaikkien tulosalueiden
  valmistumista. Workerien ohjaustiedot on erotettu 64 tavun rajoille.
- `MP blocking`: jos asynkroninen käynnistys ei onnistu, käytetään synkronista
  `StartupAllAPs()`-kutsua matvec-kohtaisesti. BSP odottaa firmwarella; vain
  valitut AP:t laskevat. Neljän loogisen prosessorin koneella tämä tarkoittaa
  enintään kolmea laskijaa. Firmwaren käynnistyskulu voi pienentää nopeushyötyä.
- `serial`: käytössä ilman sopivaa MP-tukea, komennolla `/threads 1`, tai
  synkronisen MP-kutsun epäonnistuttua. Epäonnistunut matvec lasketaan kokonaan
  uudelleen BSP:llä vasta kun firmware on lopettanut AP-kutsut.

PI määrittelee, että asynkroniset käynnistykset hylätään ReadyToBoot-tapahtuman
jälkeen. Tämä voi olla tilanne jo UEFI-sovelluksen käynnistyessä. Synkroninen
varavaihtoehto on siksi tarpeen. AP:iltä sallittu `WhoAmI()`-kutsu tunnistaa
synkronisen työn suorittajan. Muut firmware-kutsut, tulostus ja muistivaraukset
tehdään BSP:ltä. Katso [MP Services -määrittely](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/MpService.h).

AP:t alustavat MXCSR:n jokaiseen laskentatyöhön. Pool suljetaan vastauksen tai
selftestin jälkeen ja firmware-eventtien avulla varmistetaan, että kaikki AP:t
ovat palanneet ennen kuin työn muistia vapautetaan. Näppäimistöä odottaessa ei
jää pyöriviä workereita. Poolin omat tiedot ovat staattisia, eikä matvec varaa
muistia.

## Mittaus raudalla

Vertaa ensin `/threads 1` + `/selftest`, sitten `/threads 2` + `/selftest` ja
`/threads 4` + `/selftest`. Jokainen selftest aloittaa samasta neljän tokenin
tilanteesta, tulostaa valitut logittibitit ja kokonaisajan. Tokenien tulee olla
`805, 198, 2, 17`, ja logittibittien tulee säilyä samoina eri worker-määrillä.
Toista mittaus muutaman kerran ja vertaa mediaania.

Keskustelunopeuden vertailussa tee `/reset` ennen jokaista ajoa ja käytä samaa
kysymystä sekä `/tokens`-rajaa. Merkitse muistiin `Compute:`-tila, todellinen
worker-määrä, prompt-aika ja tulostetut `tok/s`-luvut. Ajat sisältävät UEFI:n
näyttötulostuksen. Tämän version worker pool tarvitsee vielä oman rautatestinsä;
aiemman yhden ytimen version käynnistys ja keskustelu on vahvistettu raudalla.

Neljä loogista prosessoria ei välttämättä tarkoita neljää fyysistä ydintä.
Kehityskoneen i5-6300U:ssa on kaksi ydintä ja neljä SMT-säiettä. Lisäksi yksi
generoitu token lukee noin gigatavun painoja: muistiväylä ja välimuistit
rajoittavat rinnakkaisuuden hyötyä. Worker-määrä kannattaa valita mittaamalla.

## Kehityskoneen testit

`make test` ajaa tuotannon MP-koodin pthread-pohjaisilla simuloiduilla
firmware-palveluilla: 1–4 workeria, epätasaiset rivijaot, tuhansia peräkkäisiä
töitä, fyysisten ytimien valinta, viallisten/estettyjen AP:iden ohitus,
poolin elinkaari sekä puuttuvan protokollan, osittaisen käynnistyksen,
eventtivirheen, synkronisen aikakatkaisun ja puuttuvan tuloksen käsittely.
Kaikki 196 608 logittia verrataan bittitasolla sarjatulokseen kahden ja neljän
workerin poolissa sekä synkronisessa tilassa, lisäksi riippumattomaan NumPyyn.
Pthreadit ovat vain kehityskoneen testiväline; EFI-ohjelmassa ei ole niitä.

`make bench` mittaa saman poolin 1, 2 ja 4 workerin läpimenon. Se lämmittää
tiedostovälimuistin ja ottaa kolmen ajon mediaanin vaihtelevassa mittausjärjestyksessä.
Host-mittaus sisältää prosessin ja poolin käynnistyksen sekä neljä täyttä
forward-kutsua, mutta ei oikean UEFI-firmwaren kustannuksia.

### Mitattu tulos 11.9.2026

i5-6300U, kaksi fyysistä ydintä / neljä loogista prosessoria, sama
SmolLM2-1.7B Q4 -malli ja kolmen ajon mediaanit:

| Workereita | Neljä forward-kutsua | Tokenia/s | Suhde sarjaversioon |
| --- | ---: | ---: | ---: |
| 1 | 6,497 s | 0,62 | 1,00× |
| 2 | 3,516 s | 1,14 | 1,85× |
| 4 | 2,687 s | 1,49 | 2,42× |

Kaikkien ajojen logitit olivat bittitasolla samat. Tämä on kehityskoneen
poolimittaus, ei rautatestin tulos eikä synkronisen UEFI-varapolun nopeuslupaus.
Seuraava optimointikohde on Q4-painojen purkamisen ja FP16-skaalojen lukemisen
kustannus sisimmässä silmukassa; sen hyöty kannattaa mitata erikseen samalla
sarja-/rinnakkaisvertailulla.
