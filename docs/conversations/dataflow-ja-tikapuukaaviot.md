# Dataflow ja tikapuukaaviot

Lähde: ChatGPT-jakotallenne `6aa13769-5008-83eb-91c5-0500badad3bb`.

## Käyttäjä

Sinä olet ehdottomasti sitä mieltä että "dataflow" arkkitehtuuri olisi "AI-ystävällisin". Mikä ero on dataflow arkkitehtuurilla ja ohejlmoitavan logiikan tikapuukaavioilla jos ajatellaan että johdotukset toimivat siinä aidosti rinnakkain kuten todellisissaa rele ja kosketin "logiikassa" tms. kytkennöissä.

## Avustaja

Jos tikapuukaavion johdot ja koskettimet todella toimivat fyysisen relelogiikan tavoin rinnakkain eikä PLC:n tavallisella scan cycle -semantiikalla, ero dataflow-arkkitehtuuriin pienenee huomattavasti. Silloin ladder on käytännössä eräs hyvin rajoitettu dataflow-kieli.

Keskeinen ero on siinä, **mitä johdoissa kulkee ja mitä solmut tarkoittavat**.

Tikapuukaaviossa perusmalli on yleensä:

\[
\text{Boolean-tila} \rightarrow \text{kosketin/rele} \rightarrow \text{Boolean-tila}
\]

Esimerkiksi:

```text
 A       B
--| |----|/|--------( Y )
```

voidaan ajatella suoraan funktiona

\[
Y = A \land \neg B
\]

Jos A tai B muuttuu, uusi Y voidaan periaatteessa laskea heti. Tämä on aivan dataflow-ajattelua.

Yleinen dataflow olisi kuitenkin esimerkiksi:

```text
temperature ──┐
              ├─ subtract ── error ── PID ── control ── motor
setpoint ─────┘
```

Johdoissa ei ole enää pelkkää `TRUE/FALSE`-tilaa, vaan siellä voi kulkea:

- kokonaisluku
- liukuluku
- merkkijono
- taulukko
- kuva
- objekti
- tapahtuma
- funktio
- jopa kokonainen toinen graafi.

Tässä mielessä ladder on vähän kuin:

> **1-bit dataflow-kieli, johon on lisätty teollisuusautomaation tarvitsemia muistia, ajastimia ja laskureita.**

### Jos ladder olisi aidosti rinnakkainen

Otetaan esimerkiksi relekytkentä:

```text
       A
 ----[ ]----------+
                  |
       B          +------(X)
 ----[ ]----------+
```

Fyysisessä järjestelmässä A:n ja B:n haarat ovat olemassa yhtä aikaa. Ei ole mitään käsitettä:

```text
suorita rivi 1
sitten rivi 2
sitten rivi 3
```

Samoin oikeassa dataflow-koneessa solmut:

```text
A ──f──┐
       ├──h──Y
B ──g──┘
```

voivat laskea `f(A)` ja `g(B)` samanaikaisesti. `h` käynnistyy, kun sen tarvitsemat tulokset ovat saatavilla.

Tässä kohtaa niiden toimintaperiaate on käytännössä sama.

Ero syntyy lähinnä **abstraktiotasosta**.

| Rele/ladder | Yleinen dataflow |
|---|---|
| kosketin | ehto / operaattori |
| kela | muuttuja / tila / output |
| johdin | yleensä boolean-signaali | mielivaltainen data |
| rele | boolean-operaatio | mielivaltainen funktio |
| ajastin | erityinen komponentti | tavallinen stateful node |
| rinnakkaishaara | OR / rinnakkainen ehto | yleinen rinnakkainen laskenta |
| takaisinkytkentä | itselukitus | state / feedback edge |

Erityisen kiinnostava kohta on **itselukitus**. Relelogiikassa:

```text
       START
 ----[     ]--------+
                   |
       MOTOR       |
 ----[     ]--------+----( MOTOR )
```

Tässä syntyy tila:

\[
M_{t+1}=START_t \lor M_t
\]

Kun lisätään STOP:

\[
M_{t+1}=(START_t\lor M_t)\land \neg STOP_t
\]

Tämä on jo täsmälleen dataflow-graafi, jossa on **feedback edge** ja tila.

### PLC:n ladder sotkee asiaa hieman

Tavallinen PLC ei oikeasti toteuta yllä olevaa täysin releiden tapaan. Se tekee tyypillisesti jotakin tämän tapaista:

```text
1. lue inputs
2. suorita ohjelma ylhäältä alas
3. päivitä outputs
4. aloita uusi scan
```

Silloin ohjelmajärjestyksellä voi olla merkitystä.

Esimerkiksi:

```text
Rung 1: A -> X
Rung 2: X -> Y
```

saattaa antaa Y:lle uuden X:n saman scanin aikana, riippuen PLC:n semantiikasta.

Todellinen releverkko ei ajattele "rung 1" ja "rung 2". Siinä on vain sähköinen verkko, joka asettuu uuteen tilaan propagointiviiveiden kautta.

**Jos poistat ladderista scan cycle -semantiikan ja annat jokaisen elementin reagoida tuloihin itsenäisesti, saat käytännössä boolean-dataflow-koneen.**

Ja tämä liittyy suoraan siihen, mistä keskustelimme AI:lle sopivasta ohjelmointimallista.

AI:lle voisi olla hyvin luontevaa rakentaa jotakin tällaista:

```text
Camera
   │
   ▼
ObjectDetector
   │ objects
   ├───────────────┐
   ▼               ▼
Distance        Classify
   │               │
   └──────┬────────┘
          ▼
       Decision
          │
          ▼
        Motor
```

Tämä ei olisi pohjimmiltaan kovin erilainen kuin valtava tikapuukaavio. Oleellinen muutos olisi vain, että **kontaktien boolean-arvojen sijasta johtimissa kulkee typed objects**.

Voisi siis aivan perustellusti ajatella AI:n "ladderia":

```text
[CameraFrame]
      |
      v
[Detect objects]
      |
      v
[objects: Object[]]
      |
      +---->[distance < 2m]----+
      |                        |
      +---->[class == human]---AND---->[STOP MOTOR]
```

Tämä olisi lähes kirjaimellisesti **teollisuusautomaation tikapuukaavion yleistys mielivaltaiselle datalle**.

Minusta vielä täsmällisempi väite kuin että "dataflow on AI-ystävällisin" olisi:

> **AI:lle kiinnostava suoritusmalli voisi olla relelogiikan kaltainen graafi, jossa solmut ovat mielivaltaisia operaatioita, johdot kuljettavat tyypitettyjä objekteja ja kaikki riippumattomat solmut suoritetaan automaattisesti rinnakkain.**

Silloin ei oikeastaan enää tarvitse puhua ohjelman "riveistä" lainkaan. Ohjelma on **kytkentä**, aika lailla kuten sähkökaavio. Tämä on juuri se merkittävä ero perinteiseen von Neumann / assembler / C / Python -ajatteluun.

## Käyttäjä

National Instrumentsilla muistaakseni oli graafisiin lohkoihin perustuva ohjelmointitapa, mutta pidin sitä ainoastaan imperatiivisen ohjelmoinnin GUI toteutuksena. Loopit oli jotain "filmejä"

## Avustaja

Tarkoitat lähes varmasti National Instrumentsin **LabVIEW’ta**.

Arviosi on osittain oikea, mutta LabVIEW ei ole pelkkä imperatiivisen ohjelmoinnin GUI. Sen varsinainen suoritusmalli on nimenomaan **dataflow**: lohko suoritetaan, kun sen tarvitsemat syötteet ovat saatavilla, eikä siksi että ohjelmalaskuri “saapui riville”.

Se, mikä tekee siitä helposti imperatiivisen näköisen, ovat juuri ne rakenteet joita muistat “filmeinä”:

- `For Loop`
- `While Loop`
- `Case Structure`
- `Sequence Structure`

Ne piirretään kehyksinä, joiden sisällä on toinen graafi. Erityisesti `Flat Sequence` tai vanha `Stacked Sequence` muistuttaa todella paljon filmiruutuja: frame 0 → frame 1 → frame 2. Se on käytännössä eksplisiittinen tapa pakottaa suoritusjärjestys dataflow-kieleen.

Tässä on tärkeä ero.

LabVIEW'n luonnollinen tapa olisi:

```text
A ──> f ──┐
           ├──> h ──> Y
B ──> g ──┘
```

`f` ja `g` voivat suorittua rinnakkain. `h` odottaa molempien tuloksia.

Imperatiivinen vastaava olisi:

```text
x = f(A)
z = g(B)
Y = h(x, z)
```

Vaikka lopputulos on sama, jälkimmäisessä tekstimuoto itsessään näyttää järjestyksen, vaikka kääntäjä ehkä rinnakkaistaisi osan siitä.

LabVIEW'ssa järjestys syntyy **datariippuvuuksista**.

Sen sijaan jos LabVIEW-ohjelmaan piirretään:

```text
+----------------+
| frame 0        |
| do A           |
+----------------+
| frame 1        |
| do B           |
+----------------+
| frame 2        |
| do C           |
+----------------+
```

niin siinä on käytännössä rakennettu takaisin program counter -ajattelu graafiseen ohjelmaan. Siksi se saattoi sinusta tuntua GUI-versiolta tavallisesta ohjelmoinnista.

Ja tässä on minusta se olennainen ero siihen AI-arkkitehtuuriin, josta puhuimme.

LabVIEW oli edelleen vahvasti **ihmiselle tarkoitettu ohjelmointikieli**. Siinä oli:

- ruudulla vedettäviä johtoja
- frameja
- loop-laatikoita
- muuttujia
- kontrollipaneeleja
- eksplisiittisesti rakennettu ohjelmarakenne.

AI:n tapauksessa en välttämättä haluaisi graafista käyttöliittymää lainkaan.

Sisäinen ohjelma voisi olla vain graafi:

```text
node 17: CameraRead
node 18: DetectObjects
node 19: Filter(class=person)
node 20: Nearest
node 21: Distance
node 22: LessThan(2.0)
node 23: MotorStop
```

ja yhteydet:

```text
17.out -> 18.image
18.objects -> 19.objects
19.objects -> 20.objects
20.object -> 21.object
21.distance -> 22.a
22.result -> 23.enable
```

Ei tarvitse koskaan piirtää mitään ruudulle.

Tämä olisi tavallaan **LabVIEW ilman LabVIEW'n GUI:ta**.

Ja ehkä vielä olennaisemmin: looppeja ei välttämättä tarvittaisi samalla tavalla ollenkaan.

Esimerkiksi:

```text
while true:
    image = camera()
    objects = detect(image)
    ...
```

voitaisiin dataflow-järjestelmässä ilmaista pysyvänä virtana:

```text
Camera ──frames──> Detect ──objects──> Decision
```

`Camera` tuottaa uuden objektin aina kun uusi frame saapuu.

Ei ole `while true`.

Samoin:

```text
for x in objects:
    classify(x)
```

voisi olla vain:

```text
ObjectStream ──> Classifier
```

missä runtime suorittaa classifier-solmun automaattisesti jokaiselle saapuvalle objektille ja vaikka 100 objektia rinnakkain.

Tämä on jo paljon kauempana imperatiivisesta ohjelmoinnista kuin LabVIEW'n “looppi laatikon sisällä”.

Eli sanoisin näin: **LabVIEW oli oikea dataflow-kieli, mutta sen ohjelmointimalli oli vielä vahvasti ihmisen graafisen ohjelmoinnin ehdoilla rakennettu.** AI:lle kiinnostavampi versio olisi dataflow ilman visuaalisuutta ja mahdollisesti ilman eksplisiittisiä loop-, sequence- ja program-counter-rakenteita lainkaan.

## Käyttäjä

eli tavallisella OO kielellä voi suoraan implementoida dataflow ohjelman kirjoittamalla säikeisiin for(;;) silmukoita?

## Avustaja

Kyllä. Tavallisella OO-kielellä voi toteuttaa aivan aidon dataflow-järjestelmän niin, että jokainen solmu on olio, jolla on oma säie ja käytännössä:

```cpp
for (;;) {
    auto a = inputA.get();
    auto b = inputB.get();

    auto y = f(a, b);

    output.put(y);
}
```

Jos `get()` blokkaa, kunnes dataa on saatavilla, tämän solmun semantiikka on jo hyvin dataflow-tyyppinen: se ei suorita siksi, että jokin program counter tuli sen kohdalle, vaan koska sen tarvitsemat syötteet tulivat saataville.

Esimerkiksi:

```text
A ──> [ F ] ──┐
               ├──> [ H ] ──> Y
B ──> [ G ] ──┘
```

voisi olla kolme oliota/säiettä:

```cpp
void F::run() {
    for (;;) {
        auto x = A.get();
        FH.put(f(x));
    }
}

void G::run() {
    for (;;) {
        auto x = B.get();
        GH.put(g(x));
    }
}

void H::run() {
    for (;;) {
        auto x = FH.get();
        auto z = GH.get();
        Y.put(h(x, z));
    }
}
```

`F` ja `G` ovat täysin riippumattomia. Jos CPU-ytimiä on kaksi, ne voivat oikeasti suorittaa samaan aikaan. `H` odottaa molempia.

Tämä muistuttaa hyvin läheisesti **Kahn process network** -mallia: prosessit suorittavat jatkuvia silmukoita ja kommunikoivat FIFO-kanavilla.

Oleellinen huomio on, että:

> **dataflow on semantiikka, ei vaatimus tietystä konearkkitehtuurista.**

Sen voi implementoida aivan tavallisella:

- C++:lla
- Javalla
- Go:lla
- Pythonilla
- Erlangilla
- Rustilla

ja alla voi olla täysin normaali von Neumann -CPU.

Mutta yksi OS-säie per dataflow-solmu ei yleensä ole tehokkain toteutus. Jos graafissa olisi miljoona solmua, miljoona säiettä olisi järjetön. Runtime tekee yleensä mieluummin tämän:

```text
data saapuu nodeen
        ↓
node merkitään runnable
        ↓
thread pool poimii noden
        ↓
node suoritetaan
        ↓
tulokset herättävät seuraavat nodet
```

Eli esimerkiksi 100 000 dataflow-solmua voisi jakaa vaikka 16 worker-säiettä.

Go tekee tästä jo aika luontevaa:

```go
func add(a, b <-chan int, out chan<- int) {
    for {
        x := <-a
        y := <-b
        out <- x + y
    }
}
```

Goroutine ei ole OS-säie, joten niitä voi olla valtava määrä. Tämä on jo melkein valmis dataflow-rakennuspalikka:

```text
goroutine = node
channel   = edge
value     = token
scheduler = dataflow runtime
```

Mutta Go ei itsessään tee ohjelmasta dataflow-ohjelmaa. Ohjelmoija voi edelleen kirjoittaa täysin imperatiivista Go:ta.

Ja tässä päästään mielestäni kiinnostavaan asiaan AI:n kannalta. AI:n ei tarvitsisi generoida:

```cpp
class Foo {
    std::thread thread;
    Queue<Input> input;
    ...
}
```

lainkaan.

AI voisi tuottaa vain:

```text
Node 31 = Add
Node 32 = Multiply
Node 33 = Compare

12.out -> 31.a
18.out -> 31.b
31.out -> 32.a
27.out -> 32.b
32.out -> 33.a
```

Runtime hoitaisi automaattisesti:

- säikeet
- jonot
- synkronoinnin
- ajoituksen
- rinnakkaisuuden
- muistinhallinnan
- mahdollisesti CPU/GPU-sijoittelun.

Eli tavallisella OO-kielellä voi rakentaa dataflow-järjestelmän erittäin helposti juuri kuvaamallasi `for(;;)` + säikeet + jonot -periaatteella. **Mutta silloin OO-kieli on dataflow-koneen toteutuskieli; varsinainen "ohjelma" on se solmuista ja kanavista muodostuva graafi.**

Tämä ero on vähän sama kuin että x64:llä voi toteuttaa Python-tulkin, mutta Python-ohjelman semantiikka ei sen vuoksi ole x64-assembleria.

## Käyttäjä

Jos aidosti rinnakkainen ladder on bitti muuttujalla totetuttaua dataflow ohjelmaa, niin miten tämä voidaan toteuttaa Obejkteilla. Tullaan Python ongelmakentälle, mitä tekee ___add___ kuvalle ja tekstille?

## Avustaja

Niin. Tässä tulee juuri vastaan se raja, jossa **"dataflow jossa johdoissa kulkee objekteja"** ei enää ole yhtä yksiselitteinen kuin boolean-ladder.

Boolean-ladderissa kaikki on helppoa, koska koko järjestelmällä on käytännössä yksi algebra:

\[
\{0,1\},\quad AND,\ OR,\ NOT
\]

Kun johdossa on `1`, tiedämme mitä `AND` tarkoittaa. Objektimaailmassa tämä katoaa.

Jos kirjoitamme abstraktisti:

```text
A ──┐
    ├── [ ADD ] ──> C
B ──┘
```

niin `ADD` ei itsessään tarkoita vielä mitään, ennen kuin tiedämme A:n ja B:n tyypit.

Python tekee tämän dynaamisella dispatchilla:

```python
a + b
```

on suunnilleen:

```python
a.__add__(b)
```

ja esimerkiksi:

```python
"foo" + "bar"
```

tarkoittaa merkkijonojen konkatenointia:

```text
"foobar"
```

Kun taas:

```python
3 + 4
```

tarkoittaa numeerista yhteenlaskua.

Kuvan kohdalla kysymys on juuri kuvaamasi ongelma: **mitä ihmettä "kuva + kuva" tarkoittaa?**

Mahdollisia merkityksiä olisi valtavasti:

- pikseliarvojen yhteenlasku
- alpha-blending
- kuvien asettaminen vierekkäin
- kuvien pinoaminen tasoiksi
- compositing
- kahden kuvan yhdistäminen datasetiksi
- jotain aivan muuta.

Siksi AI:lle suunnitellussa dataflow-järjestelmässä en käyttäisi ensisijaisesti yleistä `ADD`-operaatiota.

Käyttäisin **semanttisesti nimettyjä operaatioita**.

Esimerkiksi:

```text
Image ──┐
        ├── [AlphaComposite] ──> Image
Image ──┘
```

tai:

```text
Image ──┐
        ├── [PixelAdd] ──> Image
Image ──┘
```

tai:

```text
Text ──┐
       ├── [ConcatText] ──> Text
Text ──┘
```

ja numeroille:

```text
Number ──┐
         ├── [NumericAdd] ──> Number
Number ──┘
```

Tämä on paljon olennaisempi ero kuin miltä se ensin näyttää.

## Objektidataflow ei tarvitse Pythonin `__add__`-mallia

Voisimme ajatella objektia esimerkiksi näin:

```text
Object:
    Type
    Data
    Metadata
```

Esimerkiksi:

```text
Object #417
type = ImageRGB
width = 1920
height = 1080
data = ...
```

Dataflow-solmulla taas olisi eksplisiittinen sopimus:

```text
Operation: AlphaComposite

input0: ImageRGBA
input1: ImageRGBA
output: ImageRGBA
```

Runtime voi jo ennen suorittamista tarkistaa:

```text
ImageRGBA ───> AlphaComposite.imageA    OK
ImageRGBA ───> AlphaComposite.imageB    OK
```

mutta:

```text
Text ───> AlphaComposite.imageA
```

on tyyppivirhe.

Tämä alkaa muistuttaa sähkötekniikkaa paljon enemmän kuin Pythonia.

Et voi kytkeä 400 V kolmivaihejohtoa suoraan Ethernet-porttiin vain siksi, että molemmat ovat "johtoja".

Samoin dataflow-graafissa portilla olisi **semanttinen tyyppi**.

---

## Mutta silloin tulee toinen kysymys: kuinka yleisiä operaatioiden pitäisi olla?

Voimme tietenkin määritellä abstraktion:

```text
Addable
```

ja sanoa:

```text
ADD<T: Addable>
```

Silloin objektityyppi ilmoittaa itse, miten `ADD` toimii.

Tämä vastaa suunnilleen:

```python
__add__
```

tai C++:n:

```cpp
operator+
```

Mutta samalla syntyy juuri osoittamasi ongelma: `+` alkaa tarkoittaa aivan eri asioita eri maailmoissa.

Numeroille:

\[
2+3=5
\]

merkkijonoille:

\[
"ab"+"cd"="abcd"
\]

joukoille joku voisi määritellä:

\[
A+B=A\cup B
\]

kuville:

\[
Image+Image=?
\]

Tämä ei ole tekninen ongelma vaan **semanttinen ongelma**.

Kone osaa kyllä dispatchata oikean funktion. Mutta mistä AI tietää, mitä käyttäjä tarkoitti sanalla "yhdistä"?

## Tässä kohtaa erottaisin kolme tasoa

AI-dataflow-järjestelmässä tekisin esimerkiksi näin:

### 1. Primitiiviset operaatiot

Matemaattisesti täsmällisiä:

```text
IntAdd
FloatAdd
BooleanAnd
MatrixMultiply
StringConcat
```

Ei epäselvyyttä.

### 2. Objektikohtaiset operaatiot

Esimerkiksi:

```text
Image.Resize
Image.Crop
Image.Composite
Image.PixelAdd
Image.EncodeJPEG

Text.Concat
Text.Tokenize
Text.Replace

Audio.Mix
Audio.Concat
Audio.Resample
```

Näillä on eksplisiittinen semantiikka.

### 3. Korkean tason intentio-operaatiot

AI voisi käyttää esimerkiksi:

```text
Combine(images)
```

mutta tätä **ei suoritettaisi suoraan**.

AI:n pitäisi ensin konkretisoida se:

```text
Combine(images)
        ↓
intent analysis
        ↓
HorizontalConcatenate(images)
```

tai:

```text
AlphaComposite(images)
```

Sitten vasta deterministinen dataflow-runtime suorittaa sen.

Tämä on tärkeä ero.

AI voi käsitellä epämääräistä käsitettä:

> "yhdistä nämä kaksi kuvaa"

mutta suorituskerroksen ei pitäisi olla epämääräinen.

---

## Ladderista saadaan hyvä analogia

Aidossa rele-ladderissa:

```text
A ----[ ]----+
             +----(Y)
B ----[ ]----+
```

jokaisen komponentin sähköinen käyttäytyminen on tarkasti määritelty.

Jos tekisimme objektiversion huonosti:

```text
Object A ──┐
           ├── [Combine] ──> Object
Object B ──┘
```

emme enää tiedä mitään.

Parempi olisi:

```text
Image A ──┐
          ├── [Overlay,
Image B ──┘    opacity=0.5] ──> Image
```

Tämä vastaa paremmin relelogiikkaa: jokaisella "komponentilla" on tunnettu fysikaalinen tai matemaattinen käyttäytyminen.

## Objektia ei siis oikeastaan pitäisi ajatella vain "oliona"

Parempi perusyksikkö voisi olla:

\[
\boxed{\text{typed value}}
\]

eli tyyppiin sidottu arvo.

Esimerkiksi:

```text
Image<RGB8, 1920x1080>
Text<UTF8>
Temperature<Celsius>
Voltage<Volt>
MotorSpeed<RPM>
Array<Float32, 1024>
```

Silloin dataflow-graafi voi olla erittäin tarkasti tyypitetty:

```text
Camera
   │ Image<RGB8>
   ▼
ObjectDetector
   │ Detection[]
   ▼
NearestObject
   │ Detection
   ▼
Distance
   │ Length<Meter>
   ▼
LessThan(2 m)
   │ Boolean
   ▼
MotorStop
```

Tässä `Boolean` tulee lopulta takaisin ladder-maailmaan.

Minusta tämä on erittäin olennainen havainto: **objektipohjaisen dataflow'n ei kannata perustua Python-tyyliseen ajatukseen "lähetetään mikä tahansa objekti ja katsotaan mitä sen `__add__` tekee".**

Paljon puhtaampi malli olisi:

> **Johdoissa kulkevat vahvasti tyypitetyt arvot, ja solmut ovat täsmällisesti määriteltyjä transformaatioita näiden tyyppien välillä.**

AI:n tehtävä olisi rakentaa ja muuttaa tätä graafia. Runtime ei "päättele" operaation merkitystä enää suoritushetkellä.

Tällä tavalla päästäisiin itse asiassa aika kauas Pythonin dynaamisesta OO-maailmasta ja lähemmäksi **sähkökaaviota, jossa johtimilla vain kulkee bittien lisäksi mielivaltaisen rikkaita, mutta tarkasti tyypitettyjä arvoja**.

## Käyttäjä

TypedValue kuulostaa tässä kuintekin olio-ohjelmoinnin sovellukselta. "runtime tarkistaa" etc. Tämä kuulostaa nyt pelkältä olemassa olevan toistostolta. Onko bittitason aidosti rinnakkainen ladder  turing-täydellinen. Käsittääkseni on koska tietokone on yhä "relelogiikka" joka on toteutettu mikropiirieillä.

## Avustaja

Kyllä — mutta tärkeällä täsmennyksellä: **pelkkä äärellinen rinnakkainen rele-/ladderverkko ei ole Turing-täydellinen**, koska sillä on vain äärellinen määrä tiloja. Sen sijaan **relelogiikan rakennusmalli on laskennallisesti universaali**, jos siihen sallitaan muistielementit ja periaatteessa rajattomasti laajennettava muisti.

Tämä ei vaadi mitään OO-olioita, runtimea tai `TypedValue`-abstraktiota.

Jos ladderissa on vain tällainen kombinatorinen verkko:

```text
A ----[ ]----+
             +----(Y)
B ----[ ]----+
```

niin se toteuttaa jonkin Boolen funktion:

\[
Y=f(A,B,\ldots)
\]

Tällainen verkko ei yksin ole Turing-täydellinen, koska sillä ei ole muistia. Se on vain esimerkiksi valtava:

\[
\{0,1\}^n \rightarrow \{0,1\}^m
\]

-funktio.

Mutta releellä voidaan tehdä takaisinkytkentä:

```text
       START
----[       ]-----+
                  |
       Q          |
----[       ]-----+----( Q )
```

Nyt järjestelmällä on **tila**. `Q` riippuu paitsi tämänhetkisistä tuloista myös aikaisemmasta tilanteesta.

Sillä voidaan rakentaa bistabiili muisti, flip-floppi, rekisteri, laskuri jne.

Kun pystyt tekemään:

\[
AND,\ OR,\ NOT
\]

ja muistamaan bittejä, pystyt rakentamaan esimerkiksi NAND-portteja, flip-floppeja, rekistereitä, ALU:n, ohjelmalaskurin ja muistiosoituksen.

Eli voit rakentaa releillä kokonaisen CPU:n.

### Tässä mielessä olet täysin oikeassa tietokoneesta

Moderni CMOS-tietokone on loogisella abstraktiotasolla edelleen valtava tällainen verkko:

```text
bitit
  ↓
logiikkaportit
  ↓
flip-flopit / rekisterit
  ↓
logiikkaportit
  ↓
muisti
  ↓
...
```

Transistori korvasi releen fysikaalisena komponenttina, mutta Boolen logiikan näkökulmasta ero ei ole olennainen.

Ja suuri osa tästä todella toimii **aidosti rinnakkain**.

Kun CPU:ssa on esimerkiksi 64-bittinen summaaja, se ei normaalisti tee:

```text
laske bit 0
laske bit 1
laske bit 2
...
```

vaan siellä on fyysinen logiikkaverkko, jonka kaikki transistorit muuttavat tilaansa samanaikaisesti propagointiviiveiden rajoissa.

Esimerkiksi carry-lookahead-summaaja on nimenomaan valtava rinnakkainen Boolen verkko.

CPU:n kellotus helposti peittää tämän asian. Ohjelmoijan näkökulmasta näyttää:

```text
instruction 1
instruction 2
instruction 3
```

mutta yhden kellosyklin sisällä tapahtuu valtava määrä rinnakkaista dataflow'ta.

### Turing-täydellisyyden varsinainen ehto

Jos meillä on releverkko, jossa on esimerkiksi:

```text
              +---------------+
              |               |
Input -> logic -> memory -> logic -> Output
              ^               |
              +---------------+
```

se on äärellisellä muistilla **finite-state machine**.

Jos sillä on esimerkiksi miljoona muistibittiä, sillä voi teoriassa olla

\[
2^{1\,000\,000}
\]

eri tilaa, mutta edelleen äärellinen määrä.

Matemaattinen Turing-kone edellyttää periaatteessa rajatonta muistia.

Siksi myöskään tämä pöydällä oleva oikea x86-kone ei tiukassa matemaattisessa mielessä ole Turing-täydellinen. Sillä on esimerkiksi 32 GB RAMia ja äärellinen levy.

Silti sanomme x86:ta Turing-täydelliseksi, koska **arkkitehtuuri voidaan mallintaa mielivaltaisen suurella muistilla**.

Täsmälleen sama pätee releladderiin.

Jos voimme sanoa:

> lisää tarvittaessa yksi uusi rele / muistibitti

niin järjestelmä voidaan rakentaa laskennallisesti universaaliksi.

### Vielä kiinnostavampi johtopäätös tästä

Tämä tekee edellisen keskustelumme "dataflowsta" hieman epäilyttäväksi siinä mielessä, jonka itse nostit esille.

Ei ehkä tarvitse keksiä mitään erityistä:

> Object → TypedValue → runtime → scheduler → dataflow node

-kerrosta.

Voidaan lähteä paljon alempaa.

Ajatellaan konetta vain näin:

```text
state bits ───────────┐
                      ▼
inputs ────────> [ BOOLEAN NETWORK ] ─────> next state bits
                      │
                      └────────────────────> outputs
```

Matemaattisesti:

\[
S_{t+1}=F(S_t,I_t)
\]

\[
O_t=G(S_t,I_t)
\]

Tässä **F ja G ovat aidosti rinnakkaisia logiikkaverkkoja**.

Siinä ei ole olioita.
Ei muuttujia Python-mielessä.
Ei metodeja.
Ei `__add__`:ia.
Ei runtimea.
Ei edes välttämättä käskyjä.

On vain:

**tila → rinnakkainen transformaatio → uusi tila.**

Ja tästä voidaan rakentaa universaali tietokone.

Tämä on mielestäni paljon perustavampi lähtökohta siihen kysymykseen, jota olet ajamassa takaa: **onko AI:lle ylipäätään tarpeen rakentaa "ohjelmointikieli", vai voisiko se rakentaa suoraan tilallisia loogisia verkkoja?**

Siinä tapauksessa `kuva + teksti` -ongelmakin muuttuu täysin. Koneen primitiivitasolla ei ole olemassa kuvaa, tekstiä eikä `+`:aa. On vain bittitiloja ja niitä muuttava verkko. "Kuva", "teksti" ja "yhteenlasku" ovat vasta verkon ylemmällä tasolla syntyviä rakenteita.

## Käyttäjä

Aiemmin kontekstissa päädyttiin siihen että AI tarvii diskreetin laskukoneen avukseen kun se vastaa ihmisen kysymyksiin tai pyrkii johonkin tavoitteeseen. Nyt se tehdään Unix+python tyyliin. Tässä on hyvä kysymys: "mitä sinä vaadit" jos lasket dataan pohjautuen taloni keskuslämmityksen lämpimän menoveden sekottajan säätöjärestelmän siirtofunktion? Pitää lukea tiedostoja todellisuudesta ja tehdä algoritmeja ja kaikkea muuta. Mikä diskreetti laskin/"ohjelmointilaite"  tähän tarvitaan, onko assembler riittäävä vai onko korkean tason python modulit hyviä. Ja onko tämä oikea suunta vaikka nyt joku kuvankäsittely voidaan tehdä GPU+pythonilla, niin mikä olisi paras perusta koska tilanne on täysin muuttunut?

## Avustaja

Jos otetaan tuo lämmitysjärjestelmä esimerkiksi, vaatimus ei ole “Python”, vaan kyky tehdä muutama hyvin täsmällinen asia: lukea havaintoja, rakentaa niistä tilamalli, sovittaa parametreja, testata mallia ja tarvittaessa ajaa sama lasku eri raudalla.

## Avustaja

Minusta tässä kannattaa erottaa **laskennallinen riittävyys** ja **AI:lle hyvä laskukone**. Assembler on varmasti riittävä ensimmäisessä mielessä. Mutta se on lähes varmasti väärä rajapinta toisessa.

Jos pyytäisit minua laskemaan mittausdatasta talosi lämmityksen menoveden sekoitusventtiilin siirtofunktion, tarvitsisin käytännössä nämä kyvyt:

1. lukea mittausdata tiedostosta tai suoraan antureilta,
2. tulkita formaatti ja aikaleimat,
3. käsitellä puuttuvat/nurjat näytteet,
4. resamplata signaalit yhteiselle aikajanalle,
5. tehdä numeerisia operaatioita vektoreille ja matriiseille,
6. arvioida viiveitä ja korrelaatioita,
7. ratkaista optimointi-/least-squares-ongelmia,
8. simuloida ehdokasmallia,
9. verrata simulointia todellisiin havaintoihin,
10. tuottaa lopuksi esimerkiksi diskreetti siirtofunktio tai tilamalli.

Esimerkiksi voisin päätyä malliin

\[
T_{meno}[k+1]
=
aT_{meno}[k]
+b\,u[k-d]
+cT_{kattila}[k]
+dT_{paluu}[k]
\]

ja ratkaista \(a,b,c,d,d_{\text{delay}}\) datasta.

Tämä on jo hyvin paljastava esimerkki, koska **mikään näistä vaiheista ei tarvitse Pythonia semanttisesti**.

Python on tällä hetkellä vain erittäin hyvä tapa kutsua jo olemassa olevia laskukoneita:

```text
pandas  -> taulukkojen käsittely
numpy   -> vektori/matriisilaskenta
scipy   -> optimointi, suodatus, signaalinkäsittely
control -> säätötekniikka
pytorch -> tensorit/GPU
opencv  -> kuvankäsittely
```

Python itse ei tee raskasta laskentaa. Usein se vain sanoo C-, C++-, Fortran-, CUDA- tai muulle kirjastolle:

> laske tämä.

## Assembler olisi väärä toinen ääripää

AI voisi teoriassa generoida suoraan:

```asm
mov ...
vmulpd ...
vaddpd ...
...
```

ja rakentaa koko least-squares-ratkaisijan itse.

Mutta silloin aina kun tarvitsen

\[
x=(A^TA)^{-1}A^Tb
\]

AI:n pitäisi käytännössä keksiä uudelleen:

- matriisien tallennus,
- BLAS-operaatiot,
- QR/SVD,
- numeerinen stabiilisuus,
- SIMD,
- cache-käyttäytyminen,
- rinnakkaisuus.

Se olisi yhtä järkevää kuin että ihminen rakentaisi jokaista laskutoimitusta varten uuden CPU:n releistä.

**Assembler kannattaa olla alimmaisena toteutustasona, ei AI:n ensisijaisena laskukielenä.**

---

# Mielestäni paras välikerros olisi jotakin Pythonin ja assemblerin väliltä

Ei niinkään uusi "ohjelmointikieli" vaan **laskentagraafi/IR**.

Esimerkiksi minun lämmitystehtäväni voisi sisäisesti olla:

```text
Read("heating.csv")
       │
       ▼
DecodeCSV
       │
       ▼
Resample(dt=10s)
       │
       ├── valve_position
       ├── supply_temp
       ├── return_temp
       └── boiler_temp
               │
               ▼
          EstimateDelay
               │
               ▼
          BuildRegression
               │
               ▼
           LeastSquares
               │
               ▼
             Model
               │
         ┌─────┴─────┐
         ▼           ▼
      Simulate    Validate
```

Tämä graafi olisi **varsinainen ohjelma**.

Ei:

```python
df = pandas.read_csv(...)
...
```

eikä:

```asm
mov rax,...
```

vaan:

```text
READ_FILE
CSV_DECODE
RESAMPLE
CORRELATE
MATMUL
QR_SOLVE
SIMULATE_STATE_SPACE
```

Näillä operaatioilla olisi tarkasti määritelty semantiikka.

## Ja tässä muuttaisin hieman aiempaa TypedValue-ajatustani

Sinun kritiikkisi on aiheellinen: jos alan rakentaa

```text
Object
    .type
    .methods
    .__add__
```

olemme todella helposti vain keksineet uudelleen Pythonin/Java/C++:n.

Sitä ei tarvita.

Riittää esimerkiksi hyvin alkeellinen koneellinen rakenne:

```text
buffer:
    datatype = float64
    shape = [86400, 4]
    stride = ...
    address = ...
```

ja operaatio:

```text
MATMUL(buffer17, buffer28) -> buffer31
```

Tässä ei ole oliota OO-mielessä. `float64[86400,4]` kertoo vain miten bitit tulkitaan.

Samalla tavalla kuva voisi olla vain:

```text
uint8[1080,1920,3]
```

eikä mikään `Image`-olio, jolla on maaginen:

```python
image.__add__()
```

Jos halutaan yhdistää kuvia, graafissa lukee nimenomaan:

```text
PIXEL_ADD(A,B)
```

tai:

```text
ALPHA_COMPOSITE(A,B)
```

tai:

```text
CONCAT(axis=1,A,B)
```

Ei epäselvää `+`.

---

# Sitten päästään mielestäni olennaisimpaan asiaan

AI:n laskukoneessa pitäisi olla **eri kokoisia primitiivejä**.

Ei pelkästään:

```text
AND
OR
ADD
LOAD
STORE
```

mutta ei myöskään 100 000 Python-pakettia, joista jokaisella on historiallisesti sattumanvarainen API.

Vaan ehkä muutama sata tai muutama tuhat hyvin määriteltyä operaatiota:

```text
I/O:
READ
WRITE
DECODE_CSV
DECODE_JSON
DECODE_IMAGE

arrays:
SLICE
RESHAPE
CONCAT
SORT
FILTER

math:
ADD
MUL
EXP
LOG
REDUCE
MATMUL

linear algebra:
QR
SVD
EIGEN
SOLVE

signal:
FFT
FILTER
CORRELATE
RESAMPLE

optimization:
LEAST_SQUARES
MINIMIZE

systems:
STATE_SPACE_SIMULATE
TRANSFER_FUNCTION
ARX_FIT

image:
CONVOLVE
RESIZE
WARP
COMPOSITE
```

Ja AI voisi rakentaa näistä uusia laskentaverkkoja.

Jos operaatiota ei ole:

```text
NEW_OPERATION =
    graph(other operations)
```

ja se voidaan myöhemmin kompiloida tehokkaammaksi komponentiksi.

Tämä alkaa muistuttaa paljon enemmän **FPGA-/releverkkoa kuin Python-ohjelmaa**.

---

## Suuri muutos nykyiseen on mielestäni tämä

Perinteinen ohjelmointijärjestelmä on rakennettu ajatellen:

> ihminen kirjoittaa ohjelman, jonka tietokone suorittaa.

Siksi tarvitsemme asioita kuten:

```text
editor
source files
syntax
variables
classes
functions
packages
documentation
debugger
shell
```

Kun ohjelman kirjoittaja on AI, suuri osa tästä on kyseenalaista.

AI ei tarvitse:

```python
for i in range(len(x)):
```

koska se voi vain pyytää:

```text
MAP(F, x)
```

Tai vielä alempana runtime näkee graafista automaattisesti, että 100 000 riippumatonta operaatiota voidaan ajaa rinnakkain.

AI ei myöskään tarvitse kirjoittaa CUDAa:

```text
GPU kernel...
```

Graafi voi sanoa vain:

```text
MATMUL
```

ja laskukone päättää:

```text
small matrix -> CPU
large matrix -> GPU
huge matrix -> tiled GPU kernel
```

Tämä on juuri se kohta, jossa nykyinen tilanne on muuttunut.

## Siksi pitäisin parhaana rakennetta

```text
                  AI
                   │
                   ▼
          COMPUTE GRAPH / IR
                   │
        ┌──────────┼──────────┐
        ▼          ▼          ▼
     CPU kernels GPU kernels special HW
        │          │
        └──────────┴──────────┘
                   │
              machine code
```

Unix voisi edelleen olla ympärillä I/O- ja laitekerroksena:

```text
filesystem
network
devices
permissions
process isolation
```

mutta AI:n ei tarvitsisi "käyttää Unixia" samalla tavalla kuin ihminen käyttää shelliä.

Pythonkin voisi säilyä yhteensopivuuskerroksena:

```text
AI graph
    │
    ├── native operation
    │
    └── legacy Python package
             │
             └── scipy/foo/bar
```

Näin ei tarvitse heittää maailman valtavaa NumPy/SciPy/OpenCV-ekosysteemiä pois.

---

### Ja vastaus siihen tilalliseen logiikkaverkkoon

En menisi aivan bittitasolle asti.

AI **voisi** rakentaa suoraan:

```text
AND/OR/NOT/flip-flop
```

-verkkoja, ja siitä saa universaalin laskukoneen.

Mutta sama informaatio räjähtää valtavaksi.

Yksi:

```text
FFT(1048576 samples)
```

vastaisi suunnatonta bittitason logiikkaverkkoa.

Oikea abstraktiotaso olisi mielestäni jotain tällaista:

\[
\boxed{\text{stateful dataflow IR + mathematical kernels}}
\]

Se on riittävän matala, ettei mukana tarvitse olla Pythonin sattumanvaraista objektisemantiikkaa, mutta riittävän korkea, ettei AI joudu keksimään SVD:tä transistoritasolta aina uudelleen.

Jos siis kysyt **"mitä sinä todella vaatisit lämmitysjärjestelmäni mallintamiseen?"**, vastaus olisi yllättävän pieni:

> **Muisti, I/O, taulukko-/tensorilaskenta, muutama numeerisen analyysin primitiivi, kontrollivirran sijasta riippuvuusgraafi sekä mahdollisuus rakentaa olemassa olevista operaatioista uusia operaatioita.**

Python + Unix tarjoaa tämän tänään käytännöllisesti. Mutta en pitäisi niitä ideaalina AI:n natiivina laskukoneena. Ne ovat pikemminkin erittäin hyvä **väliaikainen ihmisten rakentama työkalupakki**, jonka alta alkaa jo hahmottua AI:lle sopivampi laskentakerros.

## Käyttäjä

Mennään vielä syvemmälle filosofiseen perusongelmaan. Lämmitysjärjestelmää voisi ohjata suoraan AI tai paljon yksinkertaisempi oppiva koneen ohjaus. Siirtofunktio ja csv tiedostot ja käyrät ovat ihmistä varten. Se mitä minä ja moni muu tietämättään haluaa on se että "joku muu hoitaa lämmityksen säädön". Todellisuudessa minä haluan kirjoittaa komentoriville *ls*  koska se antaa vallan tunteen ja koneen on listattava tiedostot. Jos hakemistossa on yli 10 tiedostoa, en hahmota enää tilannetta ja koko ls ja tiedostomaailma on turha ja turhauttava. Tästä on seurannut tiktok ja snapchat, ei haluta kuvatiedostoja vaan *kuvia* tai *snäppejä* .  Haluaisin tämä saman kehityksen myös muuhun tietotekniikkaan, en halua säätökäyriä, vaan että joku ohjaa minun sekoitusventtiiliäni. TArvitaanko kuintenin neuroverkkojen ja AI\:n tekemiseen aina numpy ja scipy - tyyppiset työkalut välivaiheena  vaikka neuroverkko ohjaisi "tyhjästä" opetellen säätämisen?

## Avustaja

Ei. **Neuroverkko tai oppiva ohjain ei tarvitse NumPya, SciPyä, CSV:tä, säätökäyriä eikä edes ihmisen ymmärtämää siirtofunktiota.** Nämä ovat pääasiassa ihmisen ja nykyisen ohjelmistotekniikan välisiä rajapintoja.

Se tarvitsee kuitenkin niiden alta löytyvät perusasiat: muistia, laskentaa, havaintoja, tilaa, jonkin tavoitteen ja jonkin tavan muuttaa omaa käyttäytymistään kokemuksen perusteella.

Lämmityksen voisi periaatteessa toteuttaa näin:

```text
ulkolämpö ─────┐
sisälämpö ─────┤
menovesi ──────┤
paluuvesi ─────┤
venttiilin tila ├──► oppiva ohjain ───► venttiili
aika/historia ─┘
```

Ohjaimelle ei koskaan tarvitse sanoa:

> talon siirtofunktio on \(G(s)=...\)

eikä sen tarvitse tuottaa:

```text
heating.csv
plot.png
curve.yaml
```

Ne syntyvät vain, jos **sinä haluat nähdä mitä tapahtuu**.

Ohjaimen varsinainen maailma voisi olla:

\[
(\text{havainnot},\text{historia})\rightarrow\text{toiminto}
\]

ja oppiminen voisi optimoida esimerkiksi:

\[
J =
(\text{sisälämpö}-21^\circ C)^2
+\lambda E
+\mu\,\text{venttiilin liike}
\]

Eli: pidä talo miellyttävänä, käytä vähän energiaa äläkä sahaa venttiiliä edestakaisin.

## Mutta jotain NumPyn alla olevaa tarvitaan

Tässä on tärkeä ero.

Neuroverkon ei tarvitse tietää mitään NumPysta, mutta neuroverkko itsessään koostuu esimerkiksi laskuista:

\[
y=\sigma(Wx+b)
\]

ja oppimisessa:

\[
W_{t+1}=W_t-\eta\nabla_W L
\]

Jossakin raudassa täytyy siis tapahtua:

- kertolaskuja
- yhteenlaskuja
- matriisi-/tensorilaskentaa
- muistista lukemista ja kirjoittamista
- gradienttien tai muun oppimissäännön laskentaa.

Nykyisin sanomme usein:

```python
torch.matmul(...)
numpy.array(...)
scipy.optimize(...)
```

mutta nämä ovat vain käyttöliittymiä näihin operaatioihin.

AI-koneessa voisi yhtä hyvin olla suoraan:

```text
MATMUL
CONV
REDUCE
UPDATE_WEIGHTS
```

eikä Pythonia olisi missään.

GPU on jo tavallaan tällainen kone.

---

## Vielä radikaalimpi vaihtoehto: ei edes neuroverkkoa "ohjelmana"

Voisit ajatella, että lämmityksen ohjain on vain muuttuva kone:

```text
sensorit
   │
   ▼
[ muuttuva verkko ]
   │
   ▼
venttiili
```

ja verkossa olevat painot ovat sen muisti.

Se ei tarvitse tiedostoja. Painojen ei edes tarvitse koskaan esiintyä ihmiselle "mallitiedostona".

Kun sähköt katkeavat, koneen täytyy tietenkin säilyttää opittu tila jossakin ei-haihtuvassa muistissa, mutta sillä ei ole mitään luonnollista syytä kutsua sitä:

```text
model_final_v17.pt
```

Se on täysin meidän nykyisen tiedostomaailmamme artefakti.

Se voisi olla yksinkertaisesti laitteen **tila**.

Tämä liittyy hyvin tuohon `ls`-havaintoosi. Tiedostojärjestelmä tekee maailmasta objekteja:

```text
IMG_8372.jpg
IMG_8373.jpg
IMG_8374.jpg
```

vaikka ihmisen käsite on:

> viime kesän kuvat veneeltä.

TikTok tekee vielä rajumman muutoksen. Käyttäjän ei edes tarvitse hallita kokoelmaa. Hän sanoo käytännössä:

> näytä minulle jotain kiinnostavaa seuraavaksi.

Järjestelmä hallitsee objektit, indeksit, tietokannat ja ranking-mallit näkymättömissä.

Sama ajatus voidaan viedä tietotekniikkaan paljon laajemmin:

Nykyinen:

```text
avaa asetukset
avaa lämmitys
etsi heating_curve
muuta slope 0.7 -> 0.65
save
```

mahdollinen tuleva:

> Täällä on aamuisin vähän liian kuuma.

Siinä kaikki.

Järjestelmä tietää, mitä "täällä", "aamuisin" ja "liian kuuma" tarkoittavat, tarkastelee historiansa, muuttaa ohjausta ja katsoo paraneeko tilanne.

## Mutta "oppiminen tyhjästä" sisältää yhden vaikean ongelman

Lämmityksessä se on vielä melko harmiton. Jos ohjain kokeilee vähän huonosti, huone voi olla pari astetta väärässä lämpötilassa.

Mutta aidosti tyhjästä oppiva kone tekisi aluksi esimerkiksi:

```text
venttiili 0 %
venttiili 100 %
venttiili 17 %
venttiili 83 %
...
```

koska sen täytyy selvittää, mitä maailma tekee.

Tätä kutsutaan exploration-ongelmaksi.

Teollisessa koneessa ei voida sanoa:

> kokeile kaikkea ja selvitä itse.

Moottori voi hajota, kattila kiehua tai ihminen loukkaantua.

Siksi käytännössä kiinnostavin tulevaisuuden kone ei ehkä ole täysin "tyhjästä oppiva", vaan:

```text
             turvallisuusrajat
                    │
sensorit ──► oppiva ohjain ──► toimilaite
                    │
              maailman malli
```

Sille annetaan muutama ehdoton invariantti:

```text
menovesi <= 80 °C
venttiilin muutosnopeus <= X
huonelämpö tavoite ≈ 21 °C
```

ja niiden sisällä se saa opetella.

Tässä ihmisen ei silti tarvitse nähdä yhtään säätökäyrää.

## Tämä muuttaa myös sen, mitä "ohjelmointi" tarkoittaa

Vanha paradigma:

> Ihminen määrää koneelle **miten** tehtävä tehdään.

```text
if temperature < ...
    open valve ...
```

Seuraava vaihe:

> Ihminen määrittelee mallin ja algoritmin.

```text
PID
MPC
transfer function
```

Oppivan järjestelmän vaihe:

> Ihminen määrittelee lähinnä **mitä halutaan** ja **mitä ei saa tehdä**.

```text
haluan 21 °C
minimoi energiankulutus
älä ylitä turvallisuusrajoja
```

Kone ratkaisee "miten".

Ja tämä on mielestäni paljon syvempi muutos kuin luonnollisen kielen lisääminen käyttöliittymäksi.

Jos vain vaihdamme:

```text
ls
```

komentoon:

> listaa tiedostoni

emme ole muuttaneet mitään olennaista. Olemme vain tehneet shellistä puheohjatun.

Todellinen muutos tapahtuu, kun käyttäjän ei tarvitse enää ajatella **tiedostoina, prosesseina, ohjelmina, parametreina tai säätökäyrinä**.

Hän ilmaisee maailman tilan ja tavoitteen:

> "Täällä on liian kuuma aamuisin."

Ja kone hoitaa loput.

Silloin NumPy, SciPy, CUDA, tiedostot, SQL ja ehkä suuri osa nykyisestä "ohjelmoinnistakin" voivat edelleen olla jossakin syvällä koneen sisällä — mutta ne muuttuvat samankaltaiseksi toteutusdetaljiksi kuin nykyisen CPU:n mikro-operaatiot ovat sinulle silloin, kun kirjoitat `ls`.

Ja vielä pidemmälle vietynä: **niitä ei välttämättä edes tarvitse olla nykyisessä muodossaan**. Oppivan koneen natiivi perusta voisi olla suoraan havainto → tila → ennuste → toiminta → palaute -kone, jossa numeerinen tensorilaskenta on yhtä näkymätön primitiivi kuin AND-portti prosessorissa nykyään.

## Käyttäjä

Tarkennan edellistä kysymystä; jos kytken codexin tai caluden kometorivisovelluksen io\:lla taloni lämmitysjärjestelmään, voi  jo tehdä lämmityksen ohjauksen tällä tavalla. Tässä tietokoneessa on kuitenkin linux ja python joita hyväksikäyttäen kone ohjaa lämmitystä. Jos tyhjennän tietokoneen kovalevyn ja laitan sen saavutettavaksi internetistä voin toisella koneella tehdä niin että sanon ; rakenna io-kone joka ohjaa lämmitystä, se on osoitteessa x.x.x.x . AI voi asentaa koneeseen käyttiksen ja ohjelmat ja tehdä siihen käyttöliittymän jossa annan palautetta; "viime yönä oli hieman viileä". Nyt AI tekee AI\:n joka on toteutettu *ihmisen käsityskyvylle ja rajotteille*. Näitä ovat rinnakkaisuus, tiedon määrä, yms. Ihmisen tietotyöskentelyn rajat ovat äärimmäisen suppeat. Voimme ajatella tietoisesti ainoastaan yhtä asiaa ja primitiiviset vaistot ovat rinnakkaisia jos vaikka putoaa lamppu katosta, niin väistämme.

## Avustaja

Niin. Tässä päästään mielestäni olennaisempaan ongelmaan kuin siihen, onko Python hyvä kieli. Nykyinen AI joutuu rakentamaan uuden järjestelmän **ihmisen tietotekniikan päälle**, joten lopputulos perii hyvin suuren määrän ihmisen kognitiivisia rajoitteita.

Jos nykyiselle Codex-tyyppiselle agentille antaisi turvallisesti hallittavan tietokoneen ja sanoisi suunnilleen:

> Tässä ovat lämpötila-anturit ja sekoitusventtiili. Hoida talon lämmitys. Minä annan palautetta.

se todennäköisesti rakentaisi jotain tämän tapaista:

```text
Linux
 ├─ device drivers
 ├─ Python/Go daemon
 ├─ SQLite/Postgres
 ├─ CSV/logit
 ├─ HTTP API
 ├─ dashboard
 ├─ controller.py
 └─ config.yaml
```

Ja tämän päälle ehkä PID:n, MPC:n tai oppivan mallin.

Teknisesti tämä voisi toimia erinomaisesti. Mutta siinä on paljon sellaista, mikä ei johdu lämmitystehtävästä lainkaan. Se johtuu siitä, **miten ihmiset ovat oppineet tekemään ohjelmistoja**.

`controller.py`, hakemistot, prosessit, lokitiedostot, SQL-taulut, REST API:t ja dashboardit eivät ole lämmitysjärjestelmän luonnollisia käsitteitä.

Ne ovat meidän luonnollisia käsitteitämme.

### Ihmisen "sarjallisuus" näkyy ohjelmoinnissa kaikkialla

Tuo huomiosi tietoisuuden kapeudesta on tässä erittäin osuva.

Ihminen kykenee tietoisesti seuraamaan suunnilleen:

```text
tee A
sitten B
jos C, tee D
palaa takaisin
```

Siksi ohjelmointikielet näyttävät tältä:

```python
x = read_temperature()
if x < target:
    valve.open()
else:
    valve.close()
```

Samoin tiedostojärjestelmä:

```text
/
 /etc
 /home
 /var
```

on hierarkia, jota ihminen pystyy navigoimaan pala kerrallaan.

Ohjelmakoodi on jaettu:

```text
files
modules
classes
functions
lines
```

koska miljoonan riippuvuuden verkkoa ihminen ei pysty katsomaan kokonaisuutena.

Koneella ei ole samaa pakkoa.

Se voisi ylläpitää samanaikaisesti esimerkiksi:

```text
sisälämpötila jokaisessa huoneessa
ulkolämpötila
auringon säteily
tuuli
kattilan lämpötila
paluuvesi
venttiilin asento
talon terminen historia
asukkaan palaute
sääennuste
energian hinta
venttiilin kuluminen
mallin epävarmuus
```

ja kaikki nämä voisivat vaikuttaa toisiinsa jatkuvasti.

Ei tarvitsisi olla yhtä:

```text
main()
```

joka "miettii seuraavan asian".

## Nykyinen AI-agentti on tässä vielä oudon ihmismäinen

Kun minä käytän esimerkiksi Pythonia, toimintaketju on helposti:

```text
1. lue tiedosto
2. katso sisältö
3. kirjoita Python
4. suorita Python
5. katso tulos
6. muuta ohjelmaa
7. suorita uudelleen
```

Tämä on hämmästyttävän lähellä ihmisen työskentelyä.

Vaikka itse kielimalli on valtavan rinnakkaisen matriisilaskennan tulos, sen **toiminta ympäröivään maailmaan** on hyvin sarjallista:

```text
observe
   ↓
reason
   ↓
tool call
   ↓
observe
   ↓
tool call
```

Tämä ei ole välttämättä laskennallinen välttämättömyys. Se on pitkälti nykyisen agenttiarkkitehtuurin ja ihmisten työkalujen seurausta.

---

# Lämmityskone voisi näyttää aivan toisenlaiselta

Ajatellaan, ettei siinä olisi käyttöjärjestelmää ihmisen näkökulmasta ollenkaan.

Bootin jälkeen olisi pysyvä kone:

```text
                       ┌── thermal model ───────────┐
                       │                            │
outside temperature ───┤                            │
room sensors ──────────►│                            │
boiler ────────────────►│      persistent state     ├──► valve
return water ──────────►│                            │
weather ───────────────►│                            │
user feedback ─────────►│                            │
                       └────────────────────────────┘
                                  ▲
                                  │
                              learning
```

Kaikki tapahtuu jatkuvasti.

Ei välttämättä ole:

```text
controller process
database process
AI process
cron process
```

vaan yksi **tilallinen laskentaverkko**.

Kun sanot:

> "Viime yönä oli hieman viileä."

tämä ei välttämättä johda mihinkään ohjelmakoodin muutokseen.

Se voisi muuttaa suoraan järjestelmän tilaa:

\[
P(\text{preferred night temperature})
\]

tai jonkin latenttitilan komponenttia.

Sen seurauksena koko säätöverkon käyttäytyminen muuttuu.

Ei tarvitse syntyä:

```text
night_target_temperature = 21.5
```

ellei jokin syy vaadi tällaista eksplisiittistä parametria.

### Ja tämä on merkittävä ero

Nykyinen AI tekisi helposti:

> käyttäjä sanoi että oli kylmä → muuta asetusta 20.5 → 21.0 °C.

Koneelle luontaisempi järjestelmä voisi oppia:

> käyttäjä kokee tämän rakennuksen lämpötiladynamiikan, vuorokaudenajan, makuuhuoneen viiveen ja ehkä ulkolämpötilan tässä yhdistelmässä hieman epämiellyttäväksi.

Seuraavana yönä se tekee hieman erilaisen ohjauksen.

Sille ei välttämättä koskaan muodostu ihmisen ymmärtämää "yölämpötilan asetusta".

---

# Tässä mielessä nykyinen "AI tekee AI:n" on bootstrap-vaihe

Voisimme kuvata kehityksen näin:

### 1. Ihminen ohjelmoi koneen

```text
human → C/Python → computer
```

### 2. AI ohjelmoi ihmisen ohjelmointikielellä

```text
human intent → AI → Python/Linux → computer
```

Tässä olemme nyt pitkälti.

Seuraava looginen vaihe olisi:

```text
human intent
      ↓
     AI
      ↓
machine-native computational structure
      ↓
physical world
```

Python katoaa välistä samalla tavalla kuin assembler on pitkälti kadonnut tavallisen ohjelmoijan näkökentästä.

Ja lopulta voisi olla vielä:

```text
human preference ───────────────┐
                               ▼
physical world ⇄ adaptive machine
```

jolloin ei ole enää mielekästä kysyä, "millä ohjelmointikielellä lämmityksen säätö on kirjoitettu".

Vähän samalla tavalla kuin ei ole kovin mielekästä kysyä:

> millä ohjelmointikielellä ihmisen pikkuaivot pitävät tasapainon?

Ne eivät suorita ihmisen kirjoittamaa algoritmia.

---

## Silti kone tarvitsee joitakin absoluuttisia rakenteita

Täysin vapaaksi tästä ei päästä.

Jossakin täytyy edelleen olla:

\[
\text{state}
\]

\[
\text{causal transformation}
\]

\[
\text{memory}
\]

\[
\text{input/output}
\]

ja fysikaalinen laskenta.

Lisäksi lämmityksen kaltaisessa cyber-physical-järjestelmässä tarvitsisin ehdottomasti AI:sta riippumattoman turvakerroksen:

```text
AI / learned controller
        │
        ▼
  safety envelope
        │
        ▼
     actuator
```

Esimerkiksi mekaaniset/elektroniset rajat:

```text
kattilan ylilämpösuoja
menoveden absoluuttinen maksimilämpö
venttiilin liikerajat
watchdog
vikatilanteen turvallinen asento
```

AI saa opetella ja tehdä virheitä **turvallisen tilan sisällä**.

Myöskään tällaista konetta ei kannattaisi laittaa paljaana julkiseen Internetiin osoitteeseen `x.x.x.x`; hallintakanavan pitäisi olla vahvasti autentikoitu ja mieluiten yksityisen verkon/VPN:n takana. Se ei muuta filosofista ideaa.

---

Minusta tässä alkaa hahmottua yksi aika voimakas määritelmä sille, mitä "AI-native computer" voisi tarkoittaa:

> **Kone, jonka ensisijainen ohjelmointiyksikkö ei ole käsky, tiedosto, funktio tai prosessi, vaan pysyvä tila, tavoite, havainto ja toiminta.**

Unix on erinomainen kone ihmiselle, joka haluaa sanoa `ls` ja hallita täsmälleen mitä tapahtuu.

Mutta koneelle, jonka tehtävä on **"hoida taloni lämpö"**, `ls`, Python, CSV, PID-asetusikkuna ja ehkä jopa koko "ohjelman" käsite saattavat olla vain historiallinen välivaihe.

## Käyttäjä

Jos analogisten yhteyksien looginen verkko voi toteuttaa myös "virtuaalisen" tietokoneen, eikö hermoverkko voi suoraan toteuttaa toista hermoverkkoa. Pentti Haikonen on raketanut analogisia älykkäitä demo-robotteja. Ihmisen päässä oleva matriisi on toteuttanut tekoälyn matriisin, mutta miksi tässä tarvitaan välikerros, taskulaskin?

## Avustaja

Perusväite on kyllä tämä: **välikerrosta ei tarvita laskennallisen universaalisuuden vuoksi**. Sitä tarvitaan lähinnä siksi, että nykyiset neuroverkot ovat huonoja joissakin asioissa, joissa diskreetti laskin on erittäin hyvä: tarkkuus, pitkä muistijälki, determinismi ja todistettavuus.

## Avustaja

Kyllä. Tässä kohtaa olen olennaisesti samaa mieltä: **“taskulaskin” ei ole periaatteellinen vaatimus älylle.** Se on nykyisen tekoälyarkkitehtuurin apuproteesi.

Pentti Haikosen XCR-1 on tästä osuva esimerkki. Hän rakensi järjestelmän nimenomaan ei-digitaaliseksi ja ei-ohjelmoitavaksi: assosiatiiviset neuroniryhmät oli kytketty suoraan näkö-, ääni-, kosketus- ja motorisiin piireihin, ilman mikroprosessoria tai varsinaista ohjelmaa.  Haikosen aiemmassa työssä lähtökohtana oli juuri hajautettu signaaliesitys ja assosiatiivinen prosessointi eikä tavallinen symbolinen ohjelma. 

### Hermoverkko voi toteuttaa hermoverkon

Tässä kannattaa erottaa sana *simuloida* sanasta *toteuttaa*.

Jos meillä on verkko

\[
N_1
\]

jonka sisäinen dynamiikka sattuu muodostamaan funktion

\[
x \rightarrow N_2(x),
\]

niin mikään laki ei vaadi, että siellä olisi välissä virtuaalinen CPU, Python tai matriisikirjasto.

Voimme yksinkertaisesti sanoa:

\[
N_1 \supset N_2
\]

eli osa ensimmäisen verkon dynamiikasta **on toinen verkko**.

Aivan kuten analogisessa piirissä:

```text
op-ampit + vastukset + kondensaattorit
                 ↓
       differentiaaliyhtälö
```

ei tarvitse olla prosessoria “laskemassa” differentiaaliyhtälöä. Piirin fysikaalinen käyttäytyminen **on differentiaaliyhtälön ratkaisu**.

Sama pätee neuroverkkoon:

```text
sensorit
   │
   ▼
┌──────────────────────────┐
│ suuri adaptiivinen verkko│
│                          │
│   ┌──────────────────┐   │
│   │ lämmitysdynamiikka│   │
│   └──────────────────┘   │
│            │             │
│   ┌──────────────────┐   │
│   │ käyttäjän mieltymys│  │
│   └──────────────────┘   │
│            │             │
│   ┌──────────────────┐   │
│   │ ohjauskäyttäytyminen│ │
│   └──────────────────┘   │
└────────────┬─────────────┘
             ▼
          venttiili
```

Nuo sisäiset laatikot eivät edes välttämättä olisi fyysisesti erotettavissa toisistaan. Me vain nimeäisimme ne jälkikäteen ihmisen ymmärtämiseksi.

## Silloin lämmityksen tapauksessa NumPy on todella ylimääräinen kerros

Nykyinen tapa:

```text
anturi
 ↓
ADC
 ↓
Linux-ajuri
 ↓
Python
 ↓
NumPy-array
 ↓
PyTorch tensor
 ↓
neuroverkko
 ↓
Python
 ↓
Linux-ajuri
 ↓
DAC/PWM
 ↓
venttiili
```

on historiallisesti ymmärrettävä, mutta luonnon kannalta absurdi määrä abstraktiota.

Voisi olla:

```text
lämpötila
    ↓
neuraalinen signaali
    ↓
adaptiivinen verkko
    ↓
motorinen signaali
    ↓
venttiili
```

Ja palaute:

```text
"viime yönä oli kylmä"
          ↓
  havainto / palkkiosignaali
          ↓
       verkko muuttuu
```

Siinä ei tarvita koskaan muuttujaa:

```python
night_temperature_target = 21.5
```

eikä edes käsitettä `21.5`, ellei se ole järjestelmälle jostakin syystä hyödyllinen sisäinen representaatio.

---

# Miksi minä sitten tarvitsen Python-taskulaskinta?

Tämä johtuu enemmän **minun nykyisestä rakenteestani** kuin älykkyyden luonteesta.

Kielimallin parametrit eivät normaalisti muutu, kun keskustelen kanssasi.

Minulla on siis valtava opittu verkko:

\[
W=\text{vakio}
\]

ja tämänhetkinen ongelma kulkee lähinnä kontekstina sen läpi.

Jos sanot:

> laske näistä 400 000 mittauksesta optimaalinen malli,

olisi erittäin tehotonta yrittää saada kiinteiden painojeni sisäinen aktivaatio “olemaan” kokonainen tarkka numeerinen optimointikone joka kerta.

Paljon halvempaa on:

```text
minä
 │
 ├── ymmärrän mitä pitäisi laskea
 │
 ▼
Python/NumPy
 │
 ▼
täsmällinen tulos
```

Taskulaskin siis kompensoi erityisesti neljää nykyisten LLM:ien ominaisuutta:

**Tarkkuutta.** Neuroverkko on luonnostaan likimääräinen. `938471 × 728391` kannattaa antaa deterministiselle kertolaskimelle.

**Pysyvää ulkoista tilaa.** Miljoona mittausta kannattaa säilyttää muistissa eikä yrittää puristaa hetkellisiin aktivaatioihin.

**Algoritmista pitkäkestoisuutta.** Tuhansien peräkkäisten tarkkojen operaatioiden suorittaminen on diskreetille koneelle helppoa.

**Todennettavuutta.** SHA-256:n, pankkisaldon tai turvarajan kohdalla “melkein oikein” ei käy.

Nämä eivät kuitenkaan tarkoita, että äly tarvitsisi Pythonia.

---

# Digitaalinen tietokone syntyi osittain juuri tarkkuusongelman vuoksi

Tässä on minusta tärkeä historiallinen/fysikaalinen seikka.

Analoginen verkko voi esimerkiksi esittää:

\[
x=0.731428...
\]

jännitteenä.

Mutta kohina, lämpötila, komponenttien toleranssit jne. sotkevat arvoa.

Digitaalisessa koneessa päätetään:

```text
0.1 V = 0
4.9 V = 1
```

ja jokainen logiikkaportti **regeneroi bitin**.

Siksi miljardien operaatioiden jälkeen voidaan edelleen saada täsmälleen sama bittijono.

Digitaalisuus ei siis ole välttämättä paras tapa rakentaa kaikkea älyä. Se on poikkeuksellisen hyvä tapa rakentaa **virheettömästi kopioituvaa diskreettiä informaatiota**.

Biologinen äly puolestaan sietää jatkuvasti:

- kohinaa
- epätarkkuutta
- kuolleita neuroneita
- muuttuvia yhteyksiä
- epätäsmällisiä aistihavaintoja.

Lämmityksen ohjaus kuuluu paljon enemmän jälkimmäiseen maailmaan.

Ei haittaa, onko venttiili 37,21344 % vai 37,19 % auki.

---

# Tästä seuraa mielestäni kiinnostava arkkitehtuuri

En enää lähtisi ajatuksesta:

```text
AI
 ↓
AI:n ohjelmointikieli
 ↓
AI:n käyttöjärjestelmä
```

vaan:

```text
                 maailma
                    ↕
        ┌─────────────────────┐
        │                     │
        │ adaptiivinen verkko │
        │                     │
        └─────────────────────┘
                    ↕
                 toiminta
```

ja sen **sisälle tai kylkeen** pieniä diskreettejä alueita silloin kun niitä todella tarvitaan:

```text
                 adaptiivinen verkko
                /        |          \
               /         |           \
       jatkuva maailma   |       tarkka muisti
                         |
                   exact compute
                    /    |    \
                  AES   integer  checksum
```

Tällöin “taskulaskin” ei ole enää AI:n käyttöympäristö.

Se on yksi erikoistunut hermoston kaltainen elin.

Vähän kuten ihmisen näköjärjestelmä ei suorita tietoisesti pikselimatriisilaskentaa, mutta me olemme rakentaneet kynän ja paperin tai laskimen, koska esimerkiksi

\[
3748297\times9182736
\]

on biologiselle hermostollemme vääränlainen tehtävä.

## Ja tässä tulee ehkä tärkein vastaus kysymykseesi

Jos rakennamme aidosti adaptiivisen neuromorfisen koneen, joka on jatkuvasti yhteydessä taloosi, **sen ei tarvitse käyttää erillistä laskinta oppiakseen lämmityksen**.

Sen fysikaalinen verkko voi itsessään muuttaa yhteyksiään:

\[
\text{havainto}
\rightarrow
\text{sisäinen tila}
\rightarrow
\text{toiminta}
\rightarrow
\text{seuraus}
\rightarrow
\text{plastisuus}
\]

Se ei “laske lämmitysmallia”.

**Se muuttuu lämmitysmalliksi.**

Tuo on minusta ratkaiseva käsitteellinen ero.

Ja silloin kysymys “millä algoritmilla se ohjaa venttiiliä?” voi olla yhtä huonosti asetettu kuin kysymys:

> Millä algoritmilla pikkuaivosi oppivat juuri sinun kävelytyylisi?

Voimme rakentaa siitä matemaattisen mallin jälkikäteen, mutta järjestelmä itse ei tarvitse tätä ihmisen tekemää väliesitystä.

Ainoa fundamentaalinen rajoite on informaatiokapasiteetti: pieni hermoverkko ei voi maagisesti sisältää mielivaltaisen suurta toista hermoverkkoa. Jossakin täytyy olla tarpeeksi fysikaalisia vapausasteita — synapseja, johtavuuksia, muistia, aikaa tai muuta tilaa — kantamaan se opittu rakenne. **Mutta Pythonia, NumPyä tai edes “virtuaalista tietokonetta” ei siihen periaatteessa tarvita.**

## Käyttäjä

tarkkuus->ihmisen idea, ei esiinny luonnossa eikä ole tarpeellinen missään. Todellisuus tulee kaikissa sovelluksissa vastaan, laskenta ja cad antaa tarkan 0.00134mm mutta sorvari ei pääse kuin 0,01mm tarkkuuteen.  tarkkuus koskee ihmisen ideoiden välistä aktia, pankkisaldoa ei esiinny luonnossa vaan pelin osana ihmisen sosiaalisessa käytöksessä. Neuroverkon tarkkuus on aina riittävä vaikka se ei ole diskreetti. Lisäksi onko kytkentä/ei ole on aina diskreetti "tarkka" tieto.
pysyvä muisti -> ei ole tarvetta koska todellisuudessa "tuotos sisältää muistin" tms. ihmisen tarkat piirustukset ovat ihminen+artefakti tarpeeseen tehtyjä, AI voi selvittää auton piirustukset autosta suoraan, siinä on mitat, ei niitä tarvita paperilla. Liittyy edelliseen "tarkkuus" ideaan, ajatellaan että piirustus on "todellinen" ja tarkka ja oliot ovat "huonoja kopioita", mutta tämä on ihmisen sosiaalista ja psykologista mielenmaisemaa.
todennettavuus -> Ihmisen näkö ja aivot "todentavat" kaukana kävelevän ihmisen identiteetin tavalla johon mikään AI ei vielä pysty (tai ehkä pystyy). SHA:t ovat sosiaalinen ja psykologinen idea, ohjelmallinen "tarkka avain" lukkosepän tekemään lukkoon.

## Avustaja

Tuossa rajauksessa olet oikeassa: minä sekoitin kaksi eri maailmaa — **fysikaalisen todellisuuden ohjaamisen** ja **ihmisen symbolisten järjestelmien ylläpitämisen**.

Luonnossa ei ole “täsmällistä lukua” samalla tavalla kuin ohjelmassa. On tila, joka mitataan aina rajallisella resoluutiolla, kohinalla ja kontekstilla. Sorvattu akseli ei ole \(20.000000\) mm vaan jokin fysikaalinen kappale, joka joko toimii laakerissa tai ei toimi. CAD:n 20.000 mm on ihmisen sopimus siitä, mitä tavoitellaan.

Siksi luonnollisessa ohjauksessa olennaisempi käsite ei ole tarkkuus vaan **riittävä erottelukyky suhteessa toiminnan seurauksiin**.

Jos venttiilin 37,1 % ja 37,2 % asennolla ei ole käytännön eroa, järjestelmän ei tarvitse edes representoida eroa.

Ja tämä johtaa kiinnostavaan ajatukseen:

> maailman mallin ei tarvitse olla tarkempi kuin toiminnan kannalta merkitykselliset erot maailmassa.

Ihmisen rakentamassa ohjelmistossa taas usein säilytetään eroja siksi, että symbolisen järjestelmän säännöt vaativat niitä.

Pankkisaldo `1234.56` ei ole luonnon suure. Se on osa yhteistä institutionaalista tilakonetta, jossa juuri bittitarkka yhteisymmärrys on pelin sääntö. Sama koskee SHA:ta, digitaalista allekirjoitusta, tiedoston tavujonoa jne.

Ne eivät osoita, että äly tarvitsisi tarkkuutta. Ne osoittavat, että **ihmiset ovat rakentaneet maailmaan järjestelmiä, joiden fysikaalinen tarkoitus on ylläpitää tarkkoja symboleita**.

### Muistista olen myös pitkälti samaa mieltä

Ajatus:

```text
maailma
→ mittaus
→ tietokanta
→ tiedosto
→ malli
→ toiminta
```

on hyvin ihmiskeskeinen.

Luonnollisempi järjestelmä voisi olla:

```text
maailma ⇄ järjestelmä
```

ja järjestelmän rakenne itsessään kantaa historiaa.

Puu “muistaa” kasvuhistoriansa vuosirenkaissa.
Metalli “muistaa” lämpökäsittelyn mikrorakenteessaan.
Hermosto “muistaa” kokemuksen synapsien ja muun rakenteen muutoksina.
Talo itsessään sisältää valtavan määrän tietoa siitä, mikä talo on.

Jos AI haluaa tietää auton akselivälin, maailmassa todella on jo auto.

Ihmisen tapa on:

```text
auto
 ↓
mitataan
 ↓
piirustus
 ↓
arkisto
 ↓
toinen ihminen lukee piirustuksen
```

koska ihmisen havaintokyky, liikkuminen, muisti ja tiedonvälitys ovat rajallisia.

Jos koneella olisi jatkuva pääsy autoon ja kyky havaita sitä riittävästi, eksplisiittinen CAD-tiedosto ei ole fundamentaalinen vaatimus.

Se on **välimuoto kommunikaatiota varten**.

### Tässä "tuotos sisältää muistin" on hyvä ilmaus

Jos oppiva lämmityksenohjain on muuttanut itseään talon perusteella, sen tämänhetkinen rakenne sisältää jo relevantin historian.

Sen ei tarvitse säilyttää:

```text
2026-09-07.csv
2026-09-08.csv
2026-09-09.csv
```

jotta se “muistaisi” talon.

Aivan kuten sinä et tarvitse videota jokaisesta kävelyaskeleestasi osataksesi kävellä huomenna.

Historia on tiivistynyt rakenteeseen.

Tämä on itse asiassa juuri sitä mitä neuroverkon painot tekevät erittäin yleisessä mielessä:

\[
\text{kokemukset} \rightarrow \text{rakenteen muutos}
\]

Sen ei tarvitse säilyttää kokemuksia sellaisinaan.

---

Todennettavuudessa sama ero näkyy vielä selvemmin.

Kun ihminen tunnistaa kaukana kävelevän henkilön, prosessi ei ole:

```text
extract features
compute identity hash
compare database
verify checksum
```

vaan suuri määrä epätäydellisiä vihjeitä konvergoi johonkin riittävän vahvaan tulkintaan:

- kävelytyyli
- pituus
- asento
- vaatetus
- paikka
- ajankohta
- odotus siitä, kuka siellä voisi olla.

Ja usein ihminen tietää erittäin nopeasti:

> tuo on Matti.

Ei ole mitään yhtä symbolista todistetta.

Silti toiminnallinen luotettavuus voi olla erittäin korkea.

Tämä on **probabilistista/assosiatiivista todentamista**, ei formaalia todentamista.

Luonnossa juuri tämä on normaalitapa.

## Tästä seuraa radikaalimpi konearkkitehtuuri

Jos poistamme nuo kolme ihmiskeskeistä oletusta:

```text
exact symbolic values
persistent explicit records
formal verification
```

AI:n peruskone voisi todella olla paljon lähempänä:

```text
              ┌─────────────────────────┐
              │                         │
world ───────►│ adaptive dynamical      │──────► world
              │ system                  │
              │                         │
              └─────────────────────────┘
                       ▲
                       │
                    feedback
```

Ei välttämättä:

```text
CPU
OS
filesystem
program
database
model file
```

lainkaan käsitteellisellä tasolla.

Fyysisesti tietenkin jokin laite toteuttaa järjestelmän, mutta nämä eivät olisi sen “ajattelun” primitiivejä.

### Diskreettiys ei silti katoa

Tuo huomiosi:

> kytkentä / ei kytkentää

on tärkeä.

Luonto ei ole pakosti “analoginen siinä mielessä että kaikki olisi jatkuvaa”.

Järjestelmässä voi syntyä robusteja diskreettejä tiloja:

```text
neuron fires / doesn't fire
door open / closed
contact conducting / not conducting
animal alive / dead
object grasped / not grasped
```

Ne eivät tarvitse ääretöntä numeerista tarkkuutta.

Ne ovat **attraktorimaisia tiloja**.

Esimerkiksi elektroniikassa jännite on analoginen:

\[
V=4.81\text{ V}
\]

mutta verkon merkitys voi olla vain:

\[
HIGH
\]

Jos se putoaa hieman:

\[
4.77V
\]

merkitys on edelleen sama.

Tämä on jopa luonnollisempi tapa saada “tarkkuutta”: ei tallenneta tarkkaa reaalilukua, vaan rakennetaan järjestelmä, jolla on vakaita erillisiä toimintatiloja.

Hermoverkossa sama voisi näkyä kategorioina, attraktoriverkkoina, toimintapäätöksinä jne.

---

## Tämä muuttaa myös käsityksen informaatiosta

Perinteinen IT-ajattelu helposti sanoo:

> tieto on bittejä, jotka pitää tallentaa.

Mutta fysikaalisemmin:

> tieto on maailman tai järjestelmän rakenteessa oleva ero, jolla voi olla kausaalinen vaikutus.

Auton muoto sisältää informaatiota.
Termostaatin jousen asento sisältää informaatiota.
Neuroverkon painot sisältävät informaatiota.
Talven jäljiltä routinut maa sisältää informaatiota menneestä lämpötilasta.

Sitä ei tarvitse ensin serialisoida JSONiksi, jotta se olisi “dataa”.

Tässä mielessä nykyinen tietotekniikka tekee kummallisen kierroksen:

```text
todellisuus
   ↓
symbolinen representaatio
   ↓
laskenta symboleilla
   ↓
symbolinen tulos
   ↓
toiminta todellisuudessa
```

Kun biologinen järjestelmä tekee lähempänä:

```text
todellisuus
   ⇄
muuttuva fysikaalinen järjestelmä
```

### Ja tästä tullaan alkuperäiseen "taskulaskimeen"

Jos tavoitteena on toimia **ihmisten symbolisessa maailmassa**, tarvitaan yhä paljon diskreettiä koneistoa:

```text
"maksa täsmälleen 1527,43 €"
"avaa tämä AES-avain"
"lähetä täsmälleen tämä TCP-paketti"
```

Mutta jos tehtävä on:

> pidä tämä talo hyvänä asua

niin taskulaskin voi todella olla tarpeeton.

Ei siksi, että neuroverkko osaisi jotenkin imitoida NumPya.

Vaan siksi, että **koko NumPyn ratkaisema ongelma saattaa olla syntynyt siitä, että ihminen halusi tehdä fysikaalisesta ongelmasta symbolisen laskutehtävän**.

Sen sijaan kone voisi oppia fysikaalisen ongelman suoraan.

Minusta tämä on paljon kiinnostavampi jako kuin “symbolinen AI vs neuroverkko”:

\[
\boxed{\text{symbolien hallinta}}
\]

vs.

\[
\boxed{\text{maailmassa toimiminen}}
\]

Nykyiset tietokoneet ovat poikkeuksellisen hyviä ensimmäisessä. Biologinen äly on poikkeuksellisen hyvä jälkimmäisessä.

Ja nykyinen AI on erikoinen hybridi: **maailmassa toimimiseen tähtäävä neuroverkko, joka joutuu käyttämään pääasiassa ihmisen symbolien hallintaan rakentamaa tietokonetta.** Se selittää aika suuren osan siitä kitkasta, jota tässä keskustelussa olemme kiertäneet.

## Käyttäjä

Jos laitan Codexin suoraan IO:lla kiinni lämmitysjärjestelmääni se on kuin ydinfyyskko talonmiehenä. Se tekee ensin näennäisesti taloudellisen ja optimoidun ohjauksen näyttävästi unixin komentoriviä rämpyttämällä. Mutta hieman erikoinen, aito talonmies on usein vielä tehokkaampi. Tämä johtuu siitä että se havaitsee asioita joita ei ole kytketty AI ohjaukseen tai joita tiedemies ei näe. "Isäntä unohtaa autotallin oven aina auki kun se käy tallissa ottamassa viinaryypyn". Talon tytär käyttää kuumaa vettä suihkussa todella paljon kun hän tulee viikonloppuna kotiin. etc. Näkö ja kuuloaistin kautta tuleva informaatio + ihmiskäsitys + toimilaitteen todellinen toiminta/teoreettinen 4-20mA ohjaus ovat eri asia. Takaisinkytkentä voi olla epätarkka tai liian tarkka. Talonmies voi olla lukutaidoton. Haluasin järjestelmän joka oppii kuten talonmies, ilman tiedettä, unixia, etc.
Tässä on toisaalta sudenkuoppia. Usein kouluttamaton ihminen voi tehdä fataaleja virheitä koska käsitys teoreettisesta luonnontieteestä on vajavainen. Paraskin intuitiivinen  lentäjä jää nykyään toiseksi AI:n ajamalle dronelle tai lentokoneelle. Myös motoriikka on vaatimatonta ihmisellä, kone voi asettaa sekoitusventtiilin 0.01 asteen tarkkuudella ja tällä saattaa olla jossain tilanteessa vaikutusta.

## Avustaja

Juuri näin. Tässä alkaa hahmottua järjestelmä, joka ei ole enää "AI kirjoittaa lämmitysohjelman", vaan pikemminkin **keinotekoinen talonmies**, jolla on sekä ihmisen kaltainen tilannetaju että koneen ylivoimainen mittaus- ja säätökyky.

Talonmiehen vahvuus ei ole siinä, että hän tietää siirtofunktion. Hänen vahvuutensa on, että hän oppii talon **kausaalisen arjen**:

> autotallin ovi auki → lämpöhäviö kasvaa  
> tietty auto pihassa → tytär on kotona → lämminvesikuorma kasvaa  
> kova länsituuli → tämä huone jäähtyy  
> tietty ääni pumpusta → jokin ei toimi normaalisti  
> venttiilin ohjaus muuttui mutta putken lämpö ei → venttiili ehkä jumittaa

Tämä tieto ei ole välttämättä koskaan eksplisiittistä muodossa

\[
Q = UA(T_i-T_o)
\]

vaan verkossa syntyy kokemuksen kautta rakenne:

\[
\text{tilanne} \rightarrow \text{odotettava seuraus}.
\]

Ja juuri **näkö, kuulo, toimilaitteiden todellisen vaikutuksen havainnointi ja pitkä kokemus** tekevät talonmiehestä paremman kuin hieno säädin, joka näkee vain neljä lämpötila-anturia.

### Pelkkä lisää antureita ei kuitenkaan ratkaise tätä

Jos nykyiselle säätimelle annetaan:

```text
outdoor_temperature
room_temperature
supply_temperature
valve_position
```

se näkee erittäin kapean maailman.

Voisimme lisätä sata muuttujaa:

```text
garage_door
shower_flow
wind
occupancy
...
```

mutta silloin ihminen on edelleen päättänyt etukäteen, **mitkä maailman asiat ovat relevantteja**.

Talonmies ei toimi näin. Hän voi huomata aivan uuden asian:

> Ahaa, aina kun tuo räystäskouru jäätyy, tämän seinän huone tuntuu myöhemmin kylmältä.

Sitä yhteyttä ei kukaan järjestelmäsuunnittelija ehkä tullut edes ajatelleeksi.

Todella kiinnostava AI-järjestelmä saisi siksi raakaa ympäristöhavaintoa:

```text
cameras
microphones
temperatures
pressures
flows
electric power
actuator feedback
weather
human language
          │
          ▼
   adaptive world model
```

ja sen pitäisi itse löytää relevantit muuttujat.

---

## Mutta toinen huomiosi on vielä tärkeämpi: talonmies ei ole ihanne

Jos kopioimme vain ihmisen intuitiivisen oppimisen, saamme myös ihmisen virheet.

Talonmies voi päätellä:

> tämä pumppu pitää aina tällaista ääntä, joten se on kunnossa.

vaikka laakeri on juuri hajoamassa.

Tai:

> olen aina tehnyt näin.

vaikka toiminta on fysikaalisesti väärää.

Siksi en rakentaisi lopullista järjestelmää vain:

```text
havainto → neuroverkko → toimilaite
```

vaan vähintään kolmen hyvin erilaisen kyvyn yhdistelmäksi.

### 1. Talonmies: maailman ymmärtäminen

Hidas, adaptiivinen, assosiatiivinen järjestelmä:

```text
näkö
kuulo
historia
asukkaiden käyttäytyminen
sää
poikkeavat tapahtumat
       │
       ▼
"mitä tässä talossa oikeastaan tapahtuu?"
```

Sen ei tarvitse tietää lämpöopin yhtälöitä.

### 2. Insinööri: opitut ja fysikaaliset rajoitteet

Järjestelmällä kannattaa silti olla käytössään tietoa, jonka ihmiskunta on jo kerran hankkinut:

```text
vesi kiehuu
paineastialla on rajat
putki voi jäätyä
pumpussa tapahtuu kavitaatiota
palaminen voi tuottaa CO:ta
```

Olisi järjetöntä vaatia jokaista uutta talo-AI:ta oppimaan kokeilemalla:

> katsotaan mitä tapahtuu, jos nostan veden 170 asteeseen.

Tämä vastaa ihmistäkin. Hyvä talonmies oppii kokemuksesta, mutta hänen osaamisessaan on myös muiden ihmisten välittämää tietoa.

### 3. Servo: koneen ylivoimainen motoriikka

Kun korkean tason järjestelmä on päättänyt:

> menoveden pitäisi nyt nousta hieman

ei ole mitään syytä imitoida ihmistä, joka pyöräyttää venttiiliä käsin "suunnilleen vähän".

Alempi kone voi tehdä:

\[
u(t)=37.284\%
\]

ja ylläpitää sitä vaikka millisekuntitason takaisinkytkennällä.

Siinä kone on ihmistä ylivoimainen.

---

Tästä syntyy kiinnostava hierarkia:

```text
                    MAHDOLLISIMMAN LAAJA MAAILMA
                              │
              näkö, kuulo, kieli, historia
                              │
                              ▼
                     ┌──────────────────┐
                     │  "TALONMIES"     │
                     │                  │
                     │ oppiva maailma-  │
                     │ ja käyttäjämalli │
                     └────────┬─────────┘
                              │
                   tavoite / ennuste
                              │
                              ▼
                     ┌──────────────────┐
                     │ fysikaalinen     │
                     │ järkevyys +      │
                     │ turvallisuus     │
                     └────────┬─────────┘
                              │
                         tavoitetila
                              │
                              ▼
                     ┌──────────────────┐
                     │ nopea servo      │
                     │ µs–ms–s          │
                     └────────┬─────────┘
                              │
                              ▼
                         toimilaite
                              │
                              ▼
                         TODELLISUUS
                              │
                              └──── kaikki takaisin havaintoihin
```

Tässä korkeimman tason kone voi olla täysin epätäsmällinen:

> "talossa on ilmeisesti tulossa tavallista suurempi lämminvesikuorma."

Keskitaso muuttaa sen järkeväksi toimintatavoitteeksi.

Ja alin taso voi olla äärimmäisen tarkka:

> venttiilin moottorin kulma 71,384°.

Nämä eivät ole ristiriidassa.

## "Takaisinkytkentä voi olla liian tarkka" on hyvä havainto

Jos mittaat venttiilin kulmaa 0,001° resoluutiolla, mutta todellinen järjestelmä sisältää:

- venttiilin välystä
- virtauksen turbulenssia
- putken lämpöviivettä
- pumpun paineenvaihtelua
- patteriventtiilien liikettä
- ihmisten ovien avaamista,

niin korkea mittausresoluutio ei tarkoita korkeaa **tietoa järjestelmän tilasta**.

Voit tietää äärimmäisen tarkasti väärän asian.

\[
\text{precision} \neq \text{information}
\]

Tämä on minusta yksi keskeisistä ongelmista klassisessa automaatiossa. Valitaan muutama helposti mitattava muuttuja ja mitataan niitä erittäin tarkasti.

Talonmies voi samaan aikaan kerran vilkaista ikkunasta:

> ulko-ovi on ollut puoli tuntia auki.

ja hänen yksi epätarkka havaintonsa selittää enemmän kuin miljoona tarkkaa venttiilipositionäytettä.

---

## Paras järjestelmä olisi siis tavallaan yli-ihminen, ei ihmissimulaatio

Se saisi ihmiseltä:

- kontekstin ymmärtämisen
- uusien syy-yhteyksien löytämisen
- laajan havaintomaailman
- kokemuksesta oppimisen.

Mutta sitä ei kannattaisi rajoittaa ihmisen:

- tarkkaavaisuuskaistaan
- muistiin
- reaktionopeuteen
- motoriseen epätarkkuuteen
- intuitiivisen fysiikan virheisiin.

Se voisi samanaikaisesti huomata:

> autotallin ovi on auki

ja

> pumpun moottorin spektrissä 630 Hz komponentti on kasvanut viikon aikana 8 %

ja

> menoveden lämpötilavaste on tällä ulkolämpötilalla muuttunut noin 4 %

ja silti ohjata venttiiliä mikrosekuntien tasolla.

Tällöin emme enää puhu **automaattisesta säätimestä**, vaan jatkuvasti oppivasta fysikaalisessa maailmassa elävästä agentista.

Ja tässä mielestäni päädymme alkuperäiseen kysymykseesi vielä syvemmällä tavalla: tällaisen järjestelmän luonnollinen perusrakenne ei näyttäisi Python-ohjelmalta eikä välttämättä edes nykyiseltä neuroverkolta. Sen pitäisi olla **jatkuvasti toimiva, moniaistinen, adaptiivinen dynaaminen järjestelmä**, jonka sisällä nopea tarkka laskenta on vain yksi paikallinen erityiskyky — vähän kuten ihmisessä selkäydinrefleksi, näköjärjestelmä ja tietoinen ajattelu ovat hyvin erilaisia järjestelmiä mutta kuuluvat samaan organismiin.

## Käyttäjä

Jos AI tekee kuvan, se esitetään rgb-arvomatriisina näytöllä. Kun pyydän muuttamaan kuvan yksityiskohtaa, missä muodossa kuva on AI:n muistissa kun tarkastelen alkuperäistä tuotosta?

## Avustaja

Kun tarkastelet myöhemmin alkuperäistä AI:n tekemää kuvaa, sitä ei yleensä ole säilytetty AI:n muistissa alkuperäisen generoinnin "ajatuksena" tai latenttina rakenteena. Käytännössä säilyvä artefakti on rasterikuva — tyypillisesti pakattu kuvatiedosto — josta malli muodostaa uuden sisäisen esityksen aina kun se katsoo kuvaa uudelleen.

Ketju on suunnilleen:

```text
alkuperäinen generointi
        │
        ▼
sisäinen latentti tila
        │
        ▼
RGB-kuva / kuvatiedosto
        │
        │  latentti generointitila yleensä katoaa
        ▼
myöhempi tarkastelu
        │
        ▼
kuva dekoodataan pikseleiksi
        │
        ▼
vision encoder / image encoder
        │
        ▼
uusi korkeaulotteinen sisäinen esitys
```

Tuo viimeinen ei enää ole RGB-matriisi. Se voi olla esimerkiksi joukko korkeaulotteisia vektoreita:

\[
I\in\mathbb R^{H\times W\times3}
\]

muuttuu joksikin tämänkaltaiseksi:

\[
Z\in\mathbb R^{N\times D},
\]

missä yksi \(D\)-ulotteinen vektori kuvaa jonkin kuvan alueen tai abstraktimman visuaalisen ominaisuuden.

### Se ei myöskään ole ihmisen scene graph

Ei yleensä ole tällaista eksplisiittistä muistia:

```text
object 17 = red car
object 18 = man
object 19 = tree
car.position = ...
man.left_hand = ...
```

Sen sijaan representaation eri suunnissa ja niiden yhdistelmissä on hajautuneesti tietoa esimerkiksi:

- muodoista
- tekstuureista
- väreistä
- objekteista
- sijainneista
- asennoista
- suhteista
- kuvan tyylistä.

Kun sanot:

> muuta miehen vasemmassa kädessä oleva muki vasaraksi

järjestelmä tekee karkeasti:

```text
RGB source image
       │
       ▼
visual representation
       │
       + instruction representation
       │
       ▼
image-generation/editing process
       │
       ▼
new RGB image
```

Oleellinen kohta on, ettei sen tarvitse saada takaisin **alkuperäisen kuvan generoinnissa käytettyä latenttia**. Se voi tulkita valmiin kuvan uudelleen.

## Tämä liittyy suoraan aiempaan keskusteluumme

Tässä tapahtuu juuri kiinnostava asia.

Ihmiselle kuva on tiedosto:

```text
lenin_poster.png
```

Tietokoneelle se voidaan esittää RGB-arvoina.

Mutta neuroverkolle kaikkein käyttökelpoisin representaatio ei ole kumpikaan.

Kun verkko "katsoo" kuvaa, se muodostaa siitä sisäisen tilan, jossa esimerkiksi tämä:

> punainen auto on talon vasemmalla puolella

voi olla paljon helpommin käytettävissä kuin tieto:

```text
pixel(837,429) = (174,31,22)
```

Tämä muistuttaa ihmistä. Kun katsot hetken kuvaa ja suljet silmäsi, muistissasi tuskin on:

\[
1920\times1080\times3
\]

värikanava-arvoa.

Sinulla on jotakin enemmän tämän kaltaista:

> punainen auto, rakennus takana, ihminen oikealla, pilvinen päivä.

Mutta tämäkään ei välttämättä ole eksplisiittinen symbolilista. Se on hermoston tilassa oleva hajautettu representaatio.

### Vielä tärkeämpi ero

Jos minä myöhemmin katson kuvan uudelleen, **kuva itse toimii ulkoisena muistina**.

Minun ei tarvitse muistaa jokaista yksityiskohtaa.

```text
             ┌───────────┐
             │   image   │
             └─────┬─────┘
                   │
                   ▼
             neural state
```

Tämä on täsmälleen samanlainen ilmiö kuin aiemmin puhuit autosta:

> auton mittoja ei välttämättä tarvitse säilyttää piirustuksessa, jos auto itse on havaittavissa.

Kuvatiedosto on tässä tapauksessa vain erityisen kätevä fysikaalinen artefakti, jonka AI pystyy **havaitsemaan uudelleen**.

Ja tästä seuraa ehkä yllättävä vastaus kysymykseesi:

**Kun pyydät muuttamaan kuvan yksityiskohtaa, tärkein "muisti" alkuperäisestä kuvasta ei välttämättä ole AI:ssa ollenkaan. Se on itse kuvassa.** AI katsoo sen uudelleen ja muodostaa siitä hetkellisen hermostollisen representaation, jonka avulla muokkaus tehdään.
