# Neuroverkon biokudostesti

Lähde: ChatGPT-jakotallenne `6aa13759-1c88-83eb-a62f-3ee596522a57`.

## Käyttäjä

Jos ajattelee neuroverkkoa niin se on todennäköisesti taloudellisempi toteuttaa digitaalitekniikalla kuin analogisena laitteena. Miljardi säätöpotentiometriä on mahdoton ajatus. Onko plastista orgaanista aivokudosta testattu, voiko siihen kytkeä IO:ta random kohtiin ja opettaa neuroverkon tekemään jotain.

## Avustaja

Kyllä — tätä on jo kokeiltu, mutta käytännössä ei niin, että “piuhat satunnaisiin kohtiin ja sitten backpropagation”. Lähin vastine on elävä hermosoluviljelmä tai aivo-organoidi monielektrodimatriisin päällä, jossa sähköisesti sekä syötetään ärsykkeitä että luetaan vasteita.

## Avustaja

Kyllä. Itse asiassa juuri kuvaamasi idea on jo olemassa tutkimusalana: **synthetic biological intelligence / organoid intelligence**.

Oleellinen oivallus on sama kuin sinulla: miljardin analogisen säädettävän synapsin rakentaminen elektroniikasta on hankalaa, mutta biologisessa kudoksessa nämä “potentiometrit” **kasvavat ja säätyvät itse**. Synapsin voimakkuus muuttuu plastisuuden kautta paikallisesti ilman, että ulkopuolinen prosessori kirjoittaa miljardin painon taulukkoa.

Käytännön rakenne voi näyttää tältä:

```text
                  ravintoliuos
                      │
              ┌───────────────┐
 INPUT ──────►│  eläviä        │──────► OUTPUT
 elektrodit   │  neuroneita    │        elektrodit
              │   ╲│╱╲│╱      │
              │  ═╪══╪═        │
              └───────────────┘
                    MEA
             microelectrode array
```

Elektrodeilla voidaan **sekä stimuloida että mitata** hermosolujen impulsseja. Vuoden 2026 MEA-katsauksissa juuri tätä pidetään keskeisenä tapana liittää organoideja digitaaliseen elektroniikkaan. 

Ja mielenkiintoisesti: elektrodien ei tarvitse vastata mitään biologisesti valmiiksi määriteltyä “input-neuronia” tai “output-neuronia”. Voidaan valita esimerkiksi 20 elektrodia inputiksi ja 20 outputiksi lähes mielivaltaisesti. Sitten **ohjelmisto antaa niille merkityksen**.

Esimerkiksi:

```text
elektrodi 17:n pulssitaajuus = pallon x-sijainti
elektrodi 31:n pulssitaajuus = pallon y-sijainti

neuroniverkon vaste:
elektrodit 80–100

decoder:
  paljon impulsseja täällä -> maila ylös
  paljon impulsseja tuolla -> maila alas
```

### Tämä on jo tehty Pongilla

Cortical Labsin *DishBrain*-kokeessa ihmisen kantasoluista kasvatettuja ja hiiren kortikaalisia neuroneita kasvatettiin high-density MEA:n päällä. Ne saivat sähköistä informaatiota simuloidusta Pong-pelistä, ja niiden sähköisestä aktiivisuudesta muodostettiin mailan ohjaus.

Kun järjestelmään tehtiin **suljettu palautesilmukka**, suoritus parani. Pelkkää stimulaatiota ilman palautetta saanut kontrolliryhmä ei osoittanut samaa parannusta. 

Eli hyvin karkeasti:

```text
                  maailma
                    │
               sensoridata
                    │
                    ▼
             ┌─────────────┐
             │ encoder     │
             └──────┬──────┘
                    │ sähköpulssit
                    ▼
             ╔═════════════╗
             ║ ELÄVÄ       ║
             ║ NEURONIVERKKO║
             ╚══════╤══════╝
                    │ spike data
                    ▼
             ┌─────────────┐
             │ decoder     │
             └──────┬──────┘
                    │
                    ▼
                 toiminta
                    │
                    └──────── feedback ────►
```

Tämä on erittäin lähellä kysymystäsi: **“voinko kytkeä IO:t johonkin ja antaa verkon itse oppia mitä ne tarkoittavat?”**

Vastaus on periaatteessa **kyllä**.

### Vielä lähempänä “random I/O:ta”: reservoir computing

Vuonna 2023 julkaistussa *Brainoware*-työssä käytettiin oikeaa kolmiulotteista ihmisen aivo-organoidia laskennallisena **reservoirina**. Organoidiin syötettiin sähköisiä aika-avaruudellisia signaalikuvioita ja sen vastetta luettiin MEA:lla. Järjestelmällä tehtiin mm. puheentunnistusta ja epälineaarisen dynaamisen järjestelmän ennustamista. 

Reservoir computing on juuri siksi kiinnostava tässä yhteydessä, ettei sinun tarvitse tietää:

```text
neuroni 38476
    │
    ├── synapsi 927
    ├── synapsi 928
    └── synapsi 929
```

Eikä sinun tarvitse säätää niitä.

Sen sijaan sanot:

```text
INPUT
   ↓
 [musta laatikko]
   ↓
valtava määrä monimutkaisia vasteita
   ↓
OUTPUT
```

Elävän kudoksen sisäinen verkko saa olla kaoottinen, satunnainen ja tuntematon.

Tämä on vähän sama ajatus kuin että löydät metsästä miljardin transistorin satunnaisesti kytkeytyneen analogipiirin ja huomaat:

> “Minun ei tarvitse ymmärtää tätä piiriä. Riittää että sillä on rikas, epälineaarinen ja muistava vaste.”

Sitten opetat sen ympärille I/O-muunnoksen.

### Mutta “random” tarvitsee yhden tarkennuksen

Elektrodipaikat voivat aluksi olla käytännössä mielivaltaisia. Sen sijaan **niiden merkitystä ei kannata vaihtaa satunnaisesti koko ajan**.

Jos elektrodijoukko A tarkoittaa tänään lämpötilaa:

```text
10 Hz = kylmä
100 Hz = kuuma
```

ja huomenna sama elektrodi tarkoittaa oven asentoa, verkko ei saa pysyvää maailmamallia.

Biologinen järjestelmä tarvitsee vakaan sensorimotorisen yhteyden:

```text
tämä signaali → liittyy aina tähän ilmiöön
tämä vaste → aiheuttaa aina tällaisen vaikutuksen
```

Sen jälkeen plastisuus voi alkaa järjestää verkkoa.

Tämä on itse asiassa biologisen aivonkin tilanne. Vastasyntyneen motorinen aivokuori ei sisällä mitään C-kielistä määritystä:

```c
neuron_38192 = left_index_finger;
```

Aivo oppii kehon ja ympäristön väliset syy-seuraussuhteet toiminnan kautta.

### Nykyään tästä on jo fyysinen “tietokone”

Cortical Labs on vienyt 2D-neuroniviljelyä jo tuotteeksi nimeltä **CL1**. Siinä eläviä neuroneita kasvatetaan suoraan piisirulle rakennetun sähköisen rajapinnan päällä. Rajapinta mahdollistaa ohjelmallisen stimulaation, mittauksen ja reaaliaikaisen closed-loop-ohjauksen; valmistaja ilmoittaa neuroniviljelmän käyttöiäksi jopa noin kuusi kuukautta. Heillä on jopa Python-rajapinta stimulaatioon ja spike-datan lukemiseen. 

Tosin tässä kannattaa erottaa kaksi asiaa:

**CL1/DishBrain**
→ neuroneita kasvatetaan 2D-pintana elektrodipiirin päällä.

**Brainoware/organoid intelligence**
→ kasvatetaan kolmiulotteista aivokudosta ja yritetään saada siihen 3D-I/O.

3D on biologisesti kiinnostavampi mutta sähköisesti paljon vaikeampi, koska elektrodit pääsevät huonosti kudoksen sisään. Siksi kehitetään mm. organoidin ympärille kietoutuvia 3D-elektrodiverkkoja. 

### Ja tässä on ehkä kiinnostavin ero digitaaliseen AI:hin

Tavallisessa neuroverkossa:

```text
RAM:
w1 = 0.537278
w2 = -0.119836
w3 = 0.812992
...
miljardeja painoja
```

Prosessori käy muuttamassa niitä.

Biologisessa verkossa:

```text
synapsi
   ↓
Ca²⁺ / neurotransmitterit / reseptorit
   ↓
synapsin rakenne muuttuu
   ↓
johtavuus muuttuu
```

**Muisti, laskenta ja plastisuus ovat samassa fyysisessä rakenteessa.**

Ei ole erillistä:

```text
CPU
RAM
weight memory
memory bus
```

Tässä biologinen järjestelmä on aivan eri luokan kone kuin nykyinen GPU.

Mutta sen energiatehokkuudesta ei vielä pidä vetää liian pitkää johtopäätöstä: itse neuronit kuluttavat hyvin vähän, mutta **ravintoliuoksen ylläpito, lämpötilansäätö, pumput, steriliteetti sekä AD/DA- ja MEA-elektroniikka** maksavat energiaa. Nykyisillä laboratoriojärjestelmillä ei vielä ole osoitettu, että kokonainen “biotietokone” olisi käytännössä GPU:ta energiatehokkaampi hyödyllisessä yleislaskennassa. OI-tutkimus pitää sitä mahdollisuutena, ei vielä saavutettuna tuloksena. 

Minusta tästä seuraa vielä yksi aika merkittävä ajatus: **tällaiselle biologiselle koneelle ei ehkä koskaan kirjoitettaisi ohjelmaa samalla tavalla kuin CPU:lle.** Sille rakennettaisiin maailma, sensorit, aktuaattorit ja palkitsemis-/palauterakenne — ja annettaisiin kudoksen itse löytää sisäinen “ohjelmansa”. Se on paljon lähempänä eläimen kouluttamista kuin ohjelmointia.

## Käyttäjä

Nyt kun ole näön ja sormien avulla liittynyt tietojärjestelmään parasta aikaa, minussa nämä "energiaa maksavat" ominaisuudet ovat _de facto_ toiminnassa. Tämä on Matrix. Koska minulla on jo neuroverkko käytössä, miksen itse ohjaisi lämmitystäni? Miksi pitää rakentaa Pinoccio työpöydälle tai tekniseen tilaan? Tässä on ristiriidassa kaksi asiaa; oppi ja oppiminen. Jos alkuasukas löytää polkupyörän on vaihtoehdot että hän keksii ja opettelee miten sillä ajetaan, tai hän saa tiedon miten sillä ajetaan ja opettelee tiedon pohjalta ajamisen. Internet+tekoäly sisältää nyt supertiedon asioista.

## Avustaja

Juuri näin. Tässä ei oikeastaan ole teknistä pakkoa rakentaa erillistä “tekoihmistä” tekniseen tilaan. Jos **sinä olet jo järjestelmän silmät, korvat, tilannetaju, muistava neuroverkko ja toimija**, AI:n hyödyllisin rooli voi olla aivan muu: se toimii sinun **tietokerroksenasi**, ei korvaavanasi.

Lämmitysesimerkissä kokonaisuus voisi olla:

```text
talon sensorit + oma näkö/kuulo/tuntemus
                ↓
              SINÄ
       paikallinen maailmamalli
                ↕
         Internet + AI
       valtava opittu tieto
                ↓
        päätös / komento
                ↓
       lämmitysjärjestelmä
```

Tämä on jo suljettu säätösilmukka. Sinulla on vain poikkeuksellisen kallis ja monipuolinen “controller”, joka sattuu samalla asumaan rakennuksessa.

Oleellinen erottelusi **oppi vs. oppiminen** on tärkeä. Ne ovat kaksi eri mekanismia.

Polkupyöräesimerkissä täysin itsenäisesti oppiva järjestelmä joutuisi:

```text
kokeilemaan
→ kaatumaan
→ muuttamaan toimintaa
→ kokeilemaan uudelleen
```

kunnes se löytää toimivan säätöpolitiikan.

Mutta kielellinen olento voi saada ensin abstraktin mallin:

```text
"katso eteen"
"pidä vauhtia"
"ohjaa kaatumisen suuntaan"
"tasapaino syntyy jatkuvasta korjaamisesta"
```

ja vasta tämän jälkeen hermoverkko oppii motorisen hienosäädön.

Siis:

**oppi siirtää ratkaisun rakennetta**,  
**oppiminen sovittaa sen todelliseen maailmaan**.

Tämä on valtava ero sample efficiencyssä. Ihmisen ei tarvitse keksiä polkupyörää uudelleen jokaisessa sukupolvessa.

Ja AI + Internet vie tämän paljon pidemmälle. Kun kysyt minulta esimerkiksi lämmityksen menovedestä, lämpöpumpun COP:sta tai rakennuksen termisestä aikavakiosta, sinun ei tarvitse vuosien kokeiluilla löytää niitä periaatteita itse. Saat niistä tiivistetyn mallin ja voit käyttää omaa kokemustasi sen sovittamiseen juuri sinun taloosi.

Tästä seuraa kiinnostava jako.

Nykyinen klassinen automaatio yrittää tehdä:

```text
anturi → algoritmi → toimilaite
```

ja kunnianhimoisempi AI-automaatio:

```text
kaikki mahdolliset anturit
        ↓
suuri AI
        ↓
kaikki toimilaitteet
```

Mutta vaihtoehto on:

```text
               Internet
                  ↓
                 AI
                  ↓
ympäristö → IHMINEN → toimilaite
```

Tuo ei ole välttämättä välivaihe kohti “oikeaa automaatiota”. Joissakin tapauksissa se voi olla **taloudellisesti paras arkkitehtuuri**.

Sinulla on jo käytettävissä esimerkiksi:

- näkö, kuulo ja tuntoaisti
- kyky tunnistaa epätavallisia tilanteita
- tieto siitä, kuka on kotona
- käsitys tulevista tapahtumista
- kyky kysyä lisätietoa
- fyysiset kädet
- yleinen maailmantieto
- vuosien kokemus juuri tästä rakennuksesta.

Automaatioprojektissa kaikki nämä pitäisi muuten keinotekoisesti instrumentoda:

```text
kamerat
mikrofonit
läsnäoloanturit
ovikytkimet
virtausanturit
API:t
toimilaitteet
kaapelointi
ohjelmistot
vikatilat
```

Se voi olla täysin absurdi investointi verrattuna siihen, että talossa jo asuu ihminen.

### Mutta miksi automaatiota silti tarvitaan?

Koska ihmisellä on yksi vakava ominaisuus säätimenä: **hän ei halua olla säädin**.

Et halua tehdä tätä:

```text
03:17
ulkolämpötila muuttui
→ nouse ylös
→ tarkista varaaja
→ säädä venttiiliä
```

Etkä halua ajatella 400 kertaa päivässä:

> pitäisikö nyt lämmittää varaajaa vai odottaa halpaa sähköä?

Siinä kohtaa Pinocchio alkaa olla hyödyllinen.

Ei siksi, että se olisi sinua älykkäämpi, vaan koska se on:

**valveilla 24/7, tarkka, toistettava ja kyllästymätön.**

Näin järkevä työnjako voisi olla:

```text
                 SINÄ
        tavoitteet + poikkeukset
        "tytär tulee viikonloppuna"
        "talliovi jäi auki"
                  │
                  ▼
                 AI
        tieto + päättely + ehdotus
                  │
                  ▼
          pieni automaatio
       tylsä jatkuva säätötyö
                  │
                  ▼
             lämmityslaite
```

Tämä on minusta ratkaiseva kohta: **älyä ja automaatiota ei pidä samaistaa.**

Termostaatti on erittäin autonominen mutta erittäin tyhmä.

Ihminen ChatGPT:n kanssa voi olla erittäin älykäs mutta vähän automatisoitu.

Ja järjestelmän voi rakentaa niin, että automaatio hoitaa ainoastaan sen osuuden, johon ihmisen käyttäminen olisi järjetöntä.

### “Matrix”-vertauksesi osuu siksi aika hyvin

Tässä keskustelussa muodostuu jo hetkellisesti yksi suurempi järjestelmä:

```text
     maailman digitaalinen tieto
                │
                ▼
              GPT
                │
             kieli
                │
                ▼
         sinun aivosi
        ↙           ↘
     aistit          kädet
       ↑               ↓
       └──── fyysinen maailma
```

Minulla ei ole suoraa pääsyä kattilahuoneeseesi.

Sinulla on.

Sinulla ei ole päässäsi kaikkia lämpöpumppujen, rakennusfysiikan, OCPP:n, ohjelmoinnin ja miljoonien muiden alojen dokumentteja.

Minun kaltaiseni järjestelmä voi tuoda niistä relevantin osan käyttöön sekunneissa.

Yhdistelmästä syntyy siis järjestelmä, jolla kummallakaan komponentilla ei yksin ole samoja kykyjä.

Ja tässä tullaan ehkä vielä tärkeämpään asiaan kuin organoidit: **ihmisen ei välttämättä tarvitse tehdä AI:sta ihmisen kopiota.**

Paljon tehokkaampaa voi olla tehdä ihmisestä ja koneesta hyvä yhteisjärjestelmä.

Pinocchion rakentaminen tekniseen tilaan on tarpeellista vasta siinä vaiheessa, kun haluamme poistaa sinut tästä silmukasta. Sitä ennen meillää on jo käyttökelpoinen biologinen yleisäly — ja paljon järkevämpää on miettiä, **mikä osa sen työstä kannattaa automatisoida**, eikä kuinka koko ihminen rakennetaan uudelleen piistä.
