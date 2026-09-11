# Matvec: MP-rinnakkaisuus ja AVX2

`matvec()` jakaa matriisin tulosrivit enintään neljään peräkkäiseen alueeseen.
Kukin worker lukee samoja syötevektoreita ja kirjoittaa vain omat tulosrivinsä.
SSE2- ja AVX2-polut säilyttävät alkuperäisen rivikohtaisen summausjärjestyksen.
Malli, kvantisointi ja kontekstimuistin koko eivät muutu.

## AVX2-valinta

`baremetal/cpu.c` tarkistaa CPUID:n XSAVE-, OSXSAVE-, AVX- ja AVX2-liput sekä
XCR0:n XMM/YMM-bitit. XGETBV suoritetaan vasta OSXSAVE-tarkistuksen jälkeen.
Tarkistus tehdään jokaisen matvec-rivityön suorittavalla BSP:llä tai AP:llä,
koska AP:n rekisteriasetukset voivat poiketa BSP:stä. Ohjelma ei kirjoita
CR4:ään tai XCR0:aan. Tarkistus seuraa
[Intelin AVX2-tunnistusohjetta](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf),
kohtaa 5.1.13.

AVX2-ydin purkaa kahdeksan Q4-painoa kerralla 32-bittisiksi arvoiksi ja laskee
kahdeksan tuloa 256-bittisillä vektoreilla. Välitulosten 128-bittiset puolikkaat
yhdistetään samassa järjestyksessä kuin alkuperäisessä SSE2-ytimessä.
FP16-skaala puretaan sisäisessä silmukassa ilman funktiokutsua. Kaikki 65 536
FP16-bittikuviota on verrattu vanhaan muuntimeen. FMA:ta ja F16C:tä ei tarvita;
FMA-yhdistäminen on estetty, jotta pyöristykset pysyvät samoina.

Muu EFI-ohjelma käännetään edelleen SSE2:lle. Vain `matvec_rows_avx2()` on
merkitty GCC:n AVX2-kohdeattribuutilla. Se ei kutsu firmwarea ja suorittaa
`vzeroupper`-käskyn ennen paluuta SSE2-koodiin. `/simd sse2` pakottaa vanhan
laskentaytimen; `/simd auto` palauttaa automaattisen valinnan. Viime ajon
käskykanta näytetään muodossa `AVX2`, `SSE2` tai `AVX2 + SSE2` todellisten
rivityökutsujen perusteella. Ennen ensimmäistä työtä tila on `not run`.

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

AVX2:n vertailussa pidä worker-määrä samana, esimerkiksi `/threads 4`.
Aja `/simd sse2` + `/selftest` ja sitten `/simd auto` + `/selftest`.
Vertaa aikoja sekä selftestin lopussa näkyvää todellista SIMD-tilaa.
Jos `auto` näyttää edelleen `SSE2`, jokin suorittavan ytimen CPU-/XCR0-vaatimus
puuttuu. `BSP AVX2 ready` kertoo vain BSP:stä; AP:t tarkistetaan erikseen.

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
Vertailut ajetaan sekä pakotetulla SSE2:lla että AVX2:lla, kun kehityskone tukee
AVX2:ta; automaattisen valinnan toteutunut ydin tarkistetaan myös.
`tests/simd.c` vertaa varsinaisia laskentaytimiä riippumattomaan skalaariseen
summauspuuhun. Se testaa FP16-muunnoksen, suojaussivuihin päättyvät puskurit,
kohdistamattomat syötteet, osittaiset rivialueet sekä eri AP:iltä peitetyn
AVX2-tuen. AVX2:ta ei pakoteta päälle sitä tukemattomassa testiympäristössä.
Pthreadit ovat vain kehityskoneen testiväline; EFI-ohjelmassa ei ole niitä.

`make bench` mittaa SSE2:n ja AVX2:n läpimenon 1, 2 ja 4 workerilla. Se lämmittää
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
Nämä luvut mitattiin ennen AVX2-laskentaytimen lisäämistä.

### AVX2-vertailu 11.9.2026

Sama i5-6300U ja malli, uusi `make bench`, kolmen ajon mediaanit.
Ajat koskevat neljää täyttä forward-kutsua:

| Workereita | SSE2-aika | AVX2-aika | AVX2 tokenia/s | Nopeutus samalla worker-määrällä |
| --- | ---: | ---: | ---: | ---: |
| 1 | 6,551 s | 2,070 s | 1,93 | 3,16× |
| 2 | 3,513 s | 1,150 s | 3,48 | 3,05× |
| 4 | 2,768 s | 1,239 s | 3,23 | 2,23× |

Nopeutus sisältää sekä AVX2-vektoroinnin että FP16-skaalojen kutsuttoman
purkamisen uudessa ytimessä. Kaikki logitit säilyivät bittitasolla samoina.
Tässä mittauksessa kaksi workeria oli AVX2:lla nopein: noin 5,69× yhden
workerin SSE2-tulokseen nähden. Raudalla kannattaa siksi verrata erityisesti
`/threads 2`- ja `/threads 4` -asetuksia. Kehityskoneen pthread-mittaus ei
sisällä oikean UEFI-firmwaren MP-kustannuksia eikä osoita sen YMM-tuen tilaa.
