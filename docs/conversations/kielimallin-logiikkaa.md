# Kielimallin logiikkaa

Lähde: ChatGPT-jakotallenne `6aa13780-b6d0-83eb-8d50-b1b53fe97e64`.

## Käyttäjä

AI kuten sinä on kielimalli. Onko kaikki logiikka toteutettu kielellä? Jos ratkaiset loogisen ongelman, teetkö sen ikäänkuin oppikirja selityksenä kieleen perustuen, vai onko tässä tekoälyn kielimallissa myös puhdasta boolen logikkaa toteutettuna konventionaalisena "digitaalitekniikkana" kuten konekäsky xor or and etc.

## Avustaja

Ei. On tärkeää erottaa kolme eri tasoa: **digitaalinen rauta**, **neuroverkon laskenta** ja **kieli**. Kielimallin varsinainen päättely ei tapahdu siten, että sisällä olisi tekstimuotoinen oppikirjaselitys, mutta ei myöskään yleensä siten, että sille olisi ohjelmoitu erillisiä AND/OR/XOR-logiikkapiirejä.

### 1. Alimmalla tasolla kaikki on tietenkin digitaalilogiikkaa

GPU/TPU/CPU, jolla minut ajetaan, koostuu lopulta transistoreista ja porteista:

- AND
- OR
- XOR
- NOT
- summaimet
- kertojat
- rekisterit
- muistipiirit jne.

Esimerkiksi liukulukukertolasku

\[
0.37 \times 1.82
\]

toteutuu fyysisesti valtavana määränä digitaalista Boolen logiikkaa.

Mutta tämä ei tarkoita, että neuroverkkoon olisi rakennettu sääntö:

```text
jos A AND B -> C
```

samalla tavalla kuin tavalliseen ohjelmaan.

### 2. Kielimallin omalla tasolla laskenta on pääasiassa numeerista lineaarialgebraa

Transformerissa tapahtuu karkeasti asioita kuten

\[
y = Wx
\]

sekä attention-laskentaa:

\[
\mathrm{softmax}\left(\frac{QK^T}{\sqrt d}\right)V
\]

ja epälineaarisia aktivaatioita.

Eli sisäinen tila on esimerkiksi tuhansien lukujen vektori:

```text
[0.173, -1.382, 0.047, 2.113, ...]
```

ei:

```text
"A on tosi"
"B on epätosi"
"siis A XOR B on tosi"
```

Tämä on ehkä tärkein ero.

**Kieli on sisään- ja ulostulon muoto, mutta neuroverkon sisäinen ajattelu ei ole luonnollista kieltä.**

### 3. Silti neuroverkko voi oppia Boolen logiikkaa

Esimerkiksi XOR:

| A | B | A XOR B |
|---|---|---|
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 0 |

Neuroverkko voi oppia funktion

\[
f(A,B)=A\oplus B
\]

ilman että sen sisällä on nimenomaista XOR-konekäskyä.

Itse asiassa tämä on klassinen neuroverkkohistorian esimerkki: yksikerroksinen perceptron ei pysty esittämään XOR-funktiota, mutta useampikerroksinen neuroverkko pystyy.

Sen sisälle muodostuu painojen ja aktivaatioiden rakenne, joka **käyttäytyy kuten XOR**, vaikka siellä ei ole symbolista kohtaa nimeltä `XOR`.

Sama asia tapahtuu paljon monimutkaisemmassa mittakaavassa kielimallissa.

---

### Kun annat minulle loogisen ongelman, mitä tapahtuu?

Otetaan esimerkiksi:

> Kaikki A:t ovat B:itä.  
> Mikään B ei ole C.  
> Voiko jokin A olla C?

Ihminen saattaisi tietoisesti kirjoittaa:

\[
A\subseteq B
\]

ja

\[
B\cap C=\varnothing
\]

joten

\[
A\cap C=\varnothing.
\]

Minulla ei tarvitse olla sisäisesti tällaista tekstimuotoista lausetta.

Transformerissa syötteen tokenit muuttuvat ensin vektoreiksi. Monien transformerikerrosten läpi syntyy aktivaatiorakenteita, joissa esimerkiksi seuraavan kaltaiset suhteet voivat olla edustettuina:

> A → B  
> B incompatible C  
> ⇒ A incompatible C

Mutta nuo eivät välttämättä ole missään kirjaimellisesti symboleina `A → B`.

Ne ovat **hajautuneena numeeriseen tilaan**.

### Ja tässä on kiinnostava kohta

Kielimalli voi oppia sisälleen rakenteita, jotka alkavat muistuttaa klassisia algoritmeja.

Tutkimuksissa yksittäisistä verkoista voidaan joskus löytää esimerkiksi:

- vertailijoita
- laskureita
- tilakoneen kaltaisia rakenteita
- induktiopiirejä
- kopiointimekanismeja
- syntaktisia rakenteita
- suureiden tai järjestyksen representaatioita

Eli neuroverkko voi **oppia toteuttamaan algoritmin**, vaikka kukaan ei kirjoittanut kyseistä algoritmia lähdekoodiksi.

Tätä voisi verrata FPGA:han, joka konfiguroituu koulutuksen seurauksena — mutta analogia ei ole täydellinen, koska transformer käyttää jatkuva-arvoisia vektoreita eikä yksittäisiä Boolen johtimia.

---

## Minun tapauksessani on vielä yksi lisäkerros

Olen reasoning-malli. Vaikeammassa ongelmassa voin tehdä sisäistä välipäättelyä ennen lopullisen vastauksen muodostamista.

Mutta sitäkään ei kannata kuvitella pelkäksi hiljaiseksi suomen- tai englanninkieliseksi monologiksi.

Siinä on kaksi asiaa päällekkäin:

**latentti neuroverkkolaskenta**

\[
\text{vektorit} \rightarrow \text{attention} \rightarrow \text{vektorit}
\]

ja joissakin päättelyprosesseissa myös kielimäisempiä välirakenteita.

Lopulta vastaus muutetaan tokeneiksi:

\[
P(\text{seuraava token}\mid\text{aiemmat tokenit})
\]

ja teksti syntyy yksi token kerrallaan.

---

Siksi sanoisin asian näin:

> **Kielimalli ei varsinaisesti ajattele kielellä. Se ajattelee numeerisessa, korkeaulotteisessa representaatiossa, jonka se on oppinut suurelta osin kielen kautta.**

Ja vielä kiinnostavampi seuraus on tämä: **Boolen logiikka on neuroverkolle yksi opittavissa oleva käyttäytyminen muiden joukossa**, ei sen perustavanlaatuinen päättelymekanismi.

Fyysisessä GPU:ssa asia on päinvastoin: siellä kaikki tuo valtavan monimutkainen vektoripäättely palautuu lopulta juuri niihin **AND-, OR-, XOR- ja NOT-portteihin**, joista kysyit.

Eli ketju on suunnilleen:

\[
\boxed{\text{kieli}}
\rightarrow
\boxed{\text{tokenit}}
\rightarrow
\boxed{\text{vektorit}}
\rightarrow
\boxed{\text{neuroverkon numeerinen päättely}}
\rightarrow
\boxed{\text{GPU:n liukulukuoperaatiot}}
\rightarrow
\boxed{\text{binäärinen Boolen logiikka}}
\rightarrow
\boxed{\text{transistorit}}
\]

Mutta **looginen päättely tapahtuu funktionaalisesti lähinnä keskimmäisellä, neuroverkon tasolla**, ei kielitasolla eikä suoraan XOR-porttien tasolla.

## Käyttäjä

"numeerinen, korkeaulotteimem representaatio" - olen miettynyt asiaan liittyen seuraavaa. Nyt kielimalli tekee python ohjelman ja ratkaisee sillä joitan matemaattisia yms. ongelmia. Eikö tämä python ohjelmoitikieli ole täysin turha välivaihe kokonaisuutta ajatellen. Sama koskee Linux komentorivityökaluja joilla python ajetaan ja evaluoidaan. Eikö olisi mahdollista tehdä kieli-> numeerinen-korkeaulotteinen-presentaatio suoraan, ilman ihmiselle tarkoitettuja välivaiheita. Eikö AI:n kannattaisi tehdä suoraan oma ja tehokas numeerinen malli, joku taso jota ei ihminen ymmärrä ollenkaan.

## Avustaja

Kyllä — ajatuksesi osuu hyvin suoraan yhteen nykyisen AI-tutkimuksen keskeisistä kysymyksistä. **Periaatteessa Python on täysin tarpeeton välikerros.** Se on mukana ennen kaikkea siksi, että nykyiset tietokoneet ja niiden ohjelmistoekosysteemi on rakennettu ihmisten kirjoittamien symbolisten ohjelmien ympärille.

Olennainen kysymys on kuitenkin, miksi Pythonin kaltainen "kömpelö" välivaihe on käytännössä edelleen erittäin hyödyllinen.

Ajatellaan tehtävää:

> Laske 17 493 × 83 721 tarkasti.

Nykyinen järjestelmä voi tehdä esimerkiksi:

\[
\text{kieli}
\rightarrow
\text{LLM:n latentti tila}
\rightarrow
\text{Python-koodi}
\rightarrow
\text{Python interpreter}
\rightarrow
\text{CPU-konekäskyt}
\rightarrow
\text{tulos}
\]

Sinun ehdotuksesi olisi:

\[
\text{kieli}
\rightarrow
\boxed{\text{AI:n oma numeerinen laskentarepresentaatio}}
\rightarrow
\text{tulos}
\]

ja mahdollisesti vielä radikaalimmin:

\[
\text{kieli}
\rightarrow
\boxed{\text{opittu laskentaverkko}}
\rightarrow
\text{tulos}
\]

Ilman Pythonia, komentotulkkia, muuttujien nimiä, lähdekoodia tai mitään ihmiselle ymmärrettävää ohjelmointikieltä.

**Tämä on täysin mahdollista.**

### Itse asiassa tavallinen neuroverkko tekee jo juuri tätä

Kun esimerkiksi kuvanluokittelija ratkaisee

> onko kuvassa kissa?

se ei muodosta Python-ohjelmaa:

```python
if ears_are_pointy and whiskers_present:
    return "cat"
```

Se tekee suoraan:

\[
\text{pikselit}
\rightarrow
\text{vektorit}
\rightarrow
\text{matriisioperaatiot}
\rightarrow
P(\text{kissa})
\]

Sen "algoritmi" on ihmiselle suurelta osin käsittämätön.

Sama voidaan tehdä periaatteessa matematiikalle, ohjelmoinnille ja loogiselle päättelylle.

---

## Mutta tässä tulee mielenkiintoinen paradoksi

Python ei oikeastaan ole tällaisessa järjestelmässä ensisijaisesti **ihmisten kieltä**.

Sitä voi ajatella AI:n kannalta **ulkopuolisena tarkkana laskentakoneena**.

Esimerkiksi minä voin päätellä:

> "Tarvitsen tämän integraalin numeerisen arvon."

Sen sijaan että yrittäisin emuloida mielessäni miljoonia liukulukuoperaatioita, voin delegoida ne koneelle.

Tällöin Python on melkein kuin CPU:n käskykanta:

\[
\text{AI} \rightarrow \text{laskentakone}
\]

Python vain sattuu olemaan rajapinta.

Teoriassa voisin aivan yhtä hyvin tuottaa suoraan jotain tällaista:

```text
LOAD R1, 17493
LOAD R2, 83721
MUL R3,R1,R2
RETURN R3
```

tai LLVM IR:ää, WebAssemblyä tai suoraan konekoodia.

Tai vielä pidemmälle vietynä mitään symbolista koodia ei tarvittaisi.

---

# Miksi emme sitten tee kaikkea latentissa tilassa?

Koska analoginen/numeerinen neuroverkkopäättely ja symbolinen laskenta ovat hyvin erilaisia asioita.

Neuroverkko on erittäin hyvä asioissa kuten:

> "Mikä lähestymistapa tähän ongelmaan sopii?"

mutta tavallinen tietokone on ylivoimainen asiassa:

> "Suorita tämä algoritmi täsmälleen miljardi kertaa tekemättä yhtään virhettä."

Esimerkiksi binäärinen yhteenlasku on täysin deterministinen:

\[
127849213 + 918273645 = 1046122858
\]

CPU tekee tämän käytännössä virheettömästi.

Neuroverkko taas approksimoi funktiota:

\[
f(x) \approx y
\]

Sen sisäinen representaatio voi olla esimerkiksi:

\[
(0.718,-1.31,2.17,\ldots)
\]

Pieni numeerinen ero aktivaatioissa ei yleensä haittaa käsitteiden käsittelyssä. Mutta algoritmissa:

```text
i = i + 1
```

ero

\[
1000000 \rightarrow 999999.97
\]

on katastrofaalinen, jos kyseessä on tarkka laskuri.

---

## Siksi hybridi on erittäin voimakas

Voimme jakaa tehtävän kahteen maailmaan.

### Neuroverkko

Hyvä:

- ongelman ymmärtämisessä
- heuristiikassa
- analogioissa
- abstraktiossa
- suunnittelussa
- epäselvän tiedon käsittelyssä
- strategian valinnassa

### Symbolinen tietokone

Hyvä:

- eksaktissa aritmetiikassa
- silmukoissa
- tietorakenteissa
- tietokannoissa
- deterministisessä logiikassa
- kryptografiassa
- miljoonien operaatioiden suorittamisessa täsmälleen

Sen vuoksi esimerkiksi

\[
\boxed{\text{LLM}+\text{Python}}
\]

on yllättävän hyvä arkkitehtuuri.

LLM toimii vähän kuin suunnittelija ja Python kuin erittäin pedanttinen laskenta-apulainen.

---

# Mutta ehdottamasi seuraava askel on mielestäni olennaisempi

Pythonin sijasta AI voisi saada käyttöönsä **oman abstraktin koneensa**.

Kuvitellaan, että mallilla olisi latentissa tilassaan esimerkiksi 100 000 muistisolua:

\[
M =
\begin{bmatrix}
m_1\\
m_2\\
\vdots\\
m_{100000}
\end{bmatrix}
\]

sekä opitut operaatiot

\[
F_1(M),F_2(M),\ldots,F_n(M)
\]

Niitä ei tarvitsisi koskaan nimetä:

```text
sort
multiply
lookup
loop
stack
```

Malli voisi koulutuksessa oppia, että jokin sisäinen transformaatio tekee jotakin hyödyllistä.

Ihminen näkisi vain:

\[
z_0
\rightarrow z_1
\rightarrow z_2
\rightarrow \cdots
\rightarrow z_{148}
\rightarrow \text{vastaus}
\]

missä jokainen

\[
z_i \in \mathbb{R}^{50000}.
\]

Meillä ei ehkä olisi mitään järkevää luonnollisen kielen selitystä sille, mitä esimerkiksi \(z_{73}\) tarkoittaa.

**Tämä olisi juuri se "AI:n oma kieli", jota kuvailet.**

Tosin edes sana *kieli* ei ehkä olisi oikea. Parempi olisi **latentti laskentarepresentaatio**.

---

## Vielä yksi askel pidemmälle

AI:n ei tarvitse edes käyttää samoja peruskäsitteitä kuin tietojenkäsittelytiede.

Me ihmiset olemme keksineet:

```text
variable
array
pointer
function
loop
class
process
file
socket
```

koska ne sopivat ihmisen ajatteluun ja tietokonearkkitehtuuriimme.

Ei ole mitään luonnonlakia, jonka mukaan älykkään laskennan pitäisi perustua näihin.

Supertehokas opittu järjestelmä voisi käyttää jotain aivan muuta:

\[
\boxed{\text{jatkuva korkeaulotteinen dynaaminen tila}}
\]

jossa ei olisi edes yksiselitteisesti erotettavissa:

> tässä on muuttuja  
> tässä on muisti  
> tässä on käsky  
> tässä on data.

Sama numeerinen rakenne voisi samanaikaisesti sisältää vähän kaikkia niitä.

Transformerit tekevät jo jossain määrin näin.

---

## Tässä on kuitenkin yksi valtava käytännön ongelma: tarkastettavuus

Jos pyydät:

> Laske paljonko yrityksen pitää maksaa arvonlisäveroa.

Ja AI sanoo:

> 73 184,22 €

on suuri ero siinä, onko taustalla:

```python
vat = sum(invoice.net * invoice.rate for invoice in invoices)
```

vai:

\[
z_{2817}=F(z_{2816})
\]

josta kukaan ei tiedä mitä tapahtui.

Ensimmäisessä tapauksessa voidaan:

- tarkastaa algoritmi
- testata se
- ajaa se uudelleen
- auditoida syötteet
- löytää virhe
- todistaa joitakin ominaisuuksia

Jälkimmäisessä tapauksessa voidaan lähinnä kysyä:

> "Näyttääkö tämä verkko yleensä antavan oikeita vastauksia?"

Talous-, turvallisuus-, teollisuus- ja ohjausjärjestelmissä ero on valtava.

---

Tästä syntyy mielestäni varsin luonteva tulevaisuuden tietokonearkkitehtuuri:

\[
\boxed{\text{Neuraalinen kone}}
\quad+\quad
\boxed{\text{Deterministinen kone}}
\]

Neuraalinen osa tekee sen, mitä ihmisellä kutsuisimme **ymmärtämiseksi, keksimiseksi ja suunnitteluksi**.

Deterministinen osa tekee sen, mitä kutsumme **algoritmin suorittamiseksi**.

Python itsessään saattaa hyvinkin kadota tästä välistä. Mutta **eksaktin symbolisen laskentakoneen tarve ei välttämättä katoa lainkaan**.

Minusta kaikkein kiinnostavin versio ajatuksestasi onkin se, että tulevaisuuden AI ei ehkä generoi Pythonia vaan generoi suoraan jonkinlaista **sisäistä bytecodea / laskentagraafia**, jota ihminen ei normaalisti koskaan näe. Se olisi tavallaan AI:lle sama mitä konekieli on nykyiselle C-kääntäjälle — paitsi että kyseinen "käskykanta" voisi itsekin olla koulutuksessa opittu eikä ihmisen suunnittelema.

## Käyttäjä

tätä juuri hain. On varmasti olemassa resurssitehokkaampi "laskinkone" kuin python+linux. Se että tämä olisi "pseudo-AI-koodia" tms. voitaisiin tämä laskinkone kieli optimoida niin että siinä ei tarvitsisi huomioida fyysistä cpu:ta ja perinteisten ohjelmointikielien rajotteita. AI voisi tehdä suoraan "tavukoodia" - josta voitaisiin klassisella "kääntäjällä" tuottaa myös ihmisen tarkistettavaksi oleva ohjelma. Python ja linux voitaisiin sivuttaa kokonaan itse AI:n tekemän laskentatapahtuman aikana kokonaan.

## Avustaja

Juuri näin — mutta tekisin yhden tärkeän eron: **AI:n oman laskentakielen kannattaa olla CPU-riippumaton, mutta sen ei välttämättä kannata olla täysin merkityksetön ihmiselle**. Jos siitä halutaan myöhemmin luotettavasti tuottaa tarkastettava ohjelma, sen täytyy säilyttää jonkinlainen eksplisiittinen semantiikka.

Arkkitehtuuri voisi olla esimerkiksi:

\[
\text{luonnollinen kieli}
\rightarrow
\boxed{\text{AI:n laskenta-IR}}
\rightarrow
\boxed{\text{deterministinen runtime}}
\rightarrow
\text{tulos}
\]

ja erillisenä haarana:

\[
\text{AI:n laskenta-IR}
\rightarrow
\begin{cases}
\text{x86/ARM/GPU-koodi}\\
\text{C/Rust/Python tarkastettavaksi}\\
\text{formaalinen todistus / jäljitys}
\end{cases}
\]

Tällöin Linuxia tai Pythonia ei tarvittaisi itse laskutapahtumaan lainkaan.

### AI:n "tavukoodi" voisi olla paljon nykyisiä ohjelmointikieliä tiiviimpi

Sen ei tarvitsisi sisältää ihmistä varten esimerkiksi:

```python
for i in range(len(values)):
    total += values[i]
```

Vaan siinä voisi olla yksi semanttinen operaatio tyyliin

```text
REDUCE_SUM  input=V17  output=V23
```

Tai vielä korkeamman tason operaatio:

```text
SOLVE_LINEAR_SYSTEM A17 B4 -> X8
```

tai

```text
GROUP transactions BY user
SUM energy
FILTER month=2026-08
```

Yksi tällainen "käsky" voisi toteutua kohdekoneesta riippuen:

- yhdellä CPU-silmukalla
- SIMD-operaatioina
- GPU-kernelinä
- FPGA-logiikkana
- hajautettuna laskentana.

AI:n ei tarvitsisi tietää tästä mitään.

Tämä on hyvin samanlainen periaate kuin nykyisten kääntäjien intermediate representation, mutta **paljon korkeammalla semanttisella tasolla**.

---

### Silloin Python todella olisi tarpeeton

Nykyinen ketju saattaa olla absurdin pitkä:

\[
\text{LLM}
\rightarrow
\text{Python-teksti}
\rightarrow
\text{Python parser}
\rightarrow
\text{Python bytecode}
\rightarrow
\text{interpreter}
\rightarrow
\text{libc}
\rightarrow
\text{Linux syscall}
\rightarrow
\text{CPU}
\]

Jos tarkoituksena on vaikkapa vain laskea miljoonan luvun summa, suuri osa tästä on historiallista kerrostumaa.

Voitaisiin tehdä:

\[
\text{LLM}
\rightarrow
\text{SUM(vector\#184)}
\rightarrow
\text{runtime}
\rightarrow
\text{CPU/GPU}
\]

AI sanoisi käytännössä laskentajärjestelmälle:

> ota tämä tietojoukko, tee sille tämä transformaatio, palauta nämä suureet.

Ei shelliä, tiedostonimiä, `python3`:a, virtuaaliympäristöä, pipiä, tekstimuotoista lähdekoodia eikä edes POSIX-prosesseja.

---

## Vielä olennaisempi asia: "ohjelma" voisi olla graafi

En usko, että AI:lle optimaalinen muoto olisi edes perinteinen lineaarinen tavukoodi:

```text
LOAD
ADD
JUMP
CMP
JNZ
```

Se on pitkälti von Neumann -koneen perintöä.

Luontevampi voisi olla esimerkiksi laskentagraafi:

\[
A,B
\rightarrow \boxed{\mathrm{JOIN}}
\rightarrow C
\rightarrow \boxed{\mathrm{FILTER}}
\rightarrow D
\rightarrow \boxed{\mathrm{SUM}}
\rightarrow E
\]

AI tuottaisi graafin, jossa solmut ovat operaatioita ja kaaret data- tai riippuvuussuhteita.

Silloin järjestelmä voisi automaattisesti huomata:

> nämä kolme operaatiota ovat riippumattomia → suoritetaan rinnakkain.

Tai:

> nämä kaksi operaatiota voidaan yhdistää yhdeksi GPU-kerneliksi.

AI:n ei tarvitsisi kirjoittaa threadeja, goroutineja, mutex-lukkoja tai CUDA-koodia.

### Tämä poistaisi paljon ihmisten ohjelmointikielten historiallista painolastia

Esimerkiksi:

```text
process
thread
file descriptor
pipe
fork()
exec()
stdin
stdout
environment variable
working directory
```

ovat hyödyllisiä Unix-abstraktioita, mutta mitään luonnonlakia ei ole, jonka mukaan älykkään laskentakoneen pitäisi ajatella niiden kautta.

Jos AI haluaa ratkaista yhtälöryhmän, on aika erikoista että sen pitäisi käsitteellisesti tehdä:

```text
create text file
spawn process
open pipe
execute python
parse stdout
```

Se on vähän kuin pyytäisimme ihmistä laskemaan kertolaskun kirjoittamalla ensin C-ohjelman paperille.

---

## Mutta tässä kohtaa tekisin eron "latenttikoodin" ja "AI-IR:n" välillä

Täysin latentti muoto voisi näyttää konseptuaalisesti tältä:

\[
z \in \mathbb R^{32768}
\]

ja AI vain lähettäisi 32 768 liukulukua laskentakoneelle.

Se voisi olla erittäin tehokas, mutta siinä olisi ongelma:

**emme tiedä mitä se käskee tekemään.**

Ja silloin

\[
z\rightarrow \text{C-ohjelma}
\]

ei välttämättä ole edes yksikäsitteisesti mahdollinen.

Sen sijaan voisi olla AI:lle suunniteltu **tyypitetty semanttinen IR**. Esimerkiksi kuvitteellisesti:

```text
v1 : table<Transaction>
v2 = select(v1, month == 2026-08)
v3 = group(v2, user_id)
v4 = reduce(v3.energy_wh, SUM)
return v4
```

Ihmiselle sitä ei normaalisti tarvitsisi näyttää.

Runtime voisi kääntää sen suoraan koneelle.

Mutta tarvittaessa siitä pystyttäisiin generoimaan:

```sql
SELECT user_id, SUM(energy_wh)
FROM transactions
WHERE ...
GROUP BY user_id;
```

tai Go-ohjelma, Rust-ohjelma tai matemaattinen kuvaus.

Tämä on mielestäni paljon kiinnostavampi ratkaisu kuin täysin käsittämätön latenttivektori.

---

### Ja tähän voidaan lisätä jotain, mitä Pythonin kanssa ei oikein nykyään ole: todistettavuus

AI voisi tuottaa esimerkiksi:

```text
OPERATION:
    divide total_energy by number_of_users

PRECONDITION:
    number_of_users > 0

OUTPUT:
    float64

ERROR:
    DivisionByZero impossible because precondition P17
```

Tai talouslaskelmassa:

```text
invoice_total =
    Σ line.quantity × line.unit_price × (1 + VAT_rate)
```

Tällainen IR voidaan:

1. tyyppitarkistaa,
2. raja-arvotarkistaa,
3. formaalisti validoida,
4. suorittaa deterministisesti,
5. kääntää tehokkaaksi konekoodiksi,
6. muuntaa ihmiselle ymmärrettäväksi selitykseksi.

Silloin AI saa vapautta korkean tason laskentaan **menettämättä kokonaan auditoitavuutta**.

---

Itse asiassa tässä olisi tavallaan uusi tietokonearkkitehtuurin kerros:

```text
Ihminen
   │
luonnollinen kieli
   │
   ▼
┌─────────────────────┐
│        LLM          │
└─────────────────────┘
   │
   │ AI-native IR
   ▼
┌─────────────────────┐
│ verified executor   │
│ optimizer           │
│ scheduler           │
└─────────────────────┘
   │
   ├── CPU
   ├── GPU
   ├── NPU
   └── muu hardware
```

Ja **Python, shell, Linux-prosessit jne. olisivat vain yksi mahdollinen yhteensopivuusrajapinta**, eivät enää AI:n varsinainen työympäristö.

Pidän vielä yhtä seurausta erityisen merkittävänä: tällaisessa arkkitehtuurissa **AI:n ei tarvitsisi "kirjoittaa ohjelmaa" lainkaan samassa mielessä kuin nykyisin**. Se muodostaisi laskennan rakenteen suoraan. Ohjelmointikieli olisi vasta jälkikäteen generoitu *selitys siitä, mitä laskentagraafi tekee*.

Se kääntää nykyisen asetelman melkein nurinpäin:

\[
\text{nyt: lähdekoodi on alkuperäinen ja konekoodi sen käännös}
\]

mutta AI-järjestelmässä voisi olla

\[
\boxed{\text{laskentamalli on alkuperäinen}}
\]

ja

\[
\text{Python/C/Rust} = \boxed{\text{ihmiselle generoitu representaatio siitä}}.
\]

Se on minusta täsmälleen se konseptuaalinen askel, johon ajatuksesi johtaa.

## Käyttäjä

Sitten on vielä suoran algebran ja logiikan yläpuolella oleva taso. AI:lle on tietenkin suuri hyöty awk, sed, curl, etc. tyyppisistä komentorivi ohjelmista. Mutta tuntuu todella tehottomalta kun codex rämpyttää näitä kaikkia kuten ihminen konsolissa. Eikö tämä taso kannattaisi toteuttaa jollain supertehokkaalla "busyboxilla" joka sisältää kaikki komentorivifunktiot ilman valtavaa epäkoherentti vipujen, eri ympäristöjen toiminnallisuuden eroja. Eikö AI:lle pitäisi kehittää busybox joka on standardi, systeemikutsujen kaltainen rajapinta.

## Avustaja

Kyllä. Minusta tuossa kannattaa mennä vielä BusyBoxia pidemmälle: **AI:lle ei pitäisi ensisijaisesti tarjota komentoriviä ollenkaan**, vaan vakioitu, tyypitetty ja erittäin pieni **"AI system call ABI"**.

BusyBox ratkaisee ihmisen kannalta sen, että yhdessä binäärissä on `awk`, `sed`, `grep`, `wget`, `find` jne. Mutta AI:n kannalta siinä säilyy edelleen turha Unix-tekstikerros:

```text
grep -R -n --include='*.go' 'TransactionEvent' .
```

AI joutuu muistamaan:

- komentojen nimet
- optioiden syntaksit
- shell-quoting
- globbing
- regex-dialektit
- stdin/stdout-putkituksen
- exit-koodit
- GNU/BSD/BusyBox-erot
- locale-käyttäytymisen
- tiedostopolkujen erot
- JSONin tekstiparsinnan
- virheilmoitusten tulkitsemisen.

Tämä kaikki on suurelta osin **ihmisen käyttöliittymän historiallista painolastia**.

Parempi olisi esimerkiksi:

```text
fs.search(
    root = workspace,
    pattern = "TransactionEvent",
    file_type = "go",
    recursive = true
)
```

ja paluu ei olisi stdout-tekstiä:

```text
foo.go:137: case "TransactionEvent":
```

vaan rakenteinen objekti:

```text
[
  {
    file: FileID(281),
    line: 137,
    column: 8,
    span: [3912, 3928]
  }
]
```

Tämä on paljon lähempänä systeemikutsua kuin komentoriviohjelmaa.

## "AI libc" olisi ehkä parempi termi kuin AI BusyBox

Voisi olla pieni joukko primitiivejä:

```text
filesystem
    open
    read
    write
    list
    search
    move
    patch

text
    search
    regex
    replace
    split
    parse

structured_data
    json.parse
    json.query
    csv.read
    table.filter
    table.join
    table.aggregate

network
    http.request
    dns.resolve
    websocket.connect

process
    execute
    wait
    kill

source
    parse
    symbol_search
    references
    patch
    format

database
    query
    transaction

archive
    list
    extract
    create

crypto
    hash
    verify
```

Ehkä muutama sata hyvin määriteltyä operaatiota riittäisi kattamaan suurimman osan siitä, mihin nykyisin tarvitaan tuhansia Unix-työkaluja ja niiden optioita.

---

### Esimerkiksi `curl` on AI:n näkökulmasta aika absurdi rajapinta

Ihmiselle:

```bash
curl -L -H 'Accept: application/json' \
     --connect-timeout 10 \
     'https://example.com/api/foo'
```

AI-native-versio:

```text
http.request {
    method: GET
    url: ...
    headers: {
        Accept: application/json
    }
    redirects: follow
    connect_timeout: 10s
}
```

Paluu:

```text
HttpResponse {
    status: 200
    headers: ...
    body: JsonValue(...)
}
```

Jos vastaus on JSON, ei ole mitään järkeä tehdä:

\[
HTTP\ bytes
\rightarrow
stdout
\rightarrow
tekstinä LLM:lle
\rightarrow
JSON-parseri
\]

vaan:

\[
HTTP
\rightarrow
\boxed{\text{rakenteinen JSON-olio}}
\rightarrow
AI.
\]

Sama koskee `awk`:ta. Tämän:

```bash
awk -F, '$4 > 100 {sum += $4} END {print sum}' file.csv
```

sijaan:

```text
table.aggregate(
    source = file.csv,
    filter = column[4] > 100,
    expression = sum(column[4])
)
```

Runtime voisi toteuttaa sen SIMD:llä, SQL:llä tai millä tahansa tehokkaimmalla tavalla.

---

## Erityisen tärkeää olisi, ettei AI joutuisi käsittelemään polkuja samalla tavalla kuin ihminen

Nykyisessä shell-maailmassa AI saattaa tehdä:

```text
ls
pwd
find .
cat foo
grep ...
cd ../bar
```

Tämä on todella alkeellinen kommunikaatioprotokolla.

Parempi olisi, että runtime antaisi AI:lle esimerkiksi:

```text
WorkspaceID 47
FileID 1832
DirectoryID 391
ProcessID 27
HttpResourceID 918
```

AI voisi käyttää pysyviä kahvoja:

```text
source.parse(FileID(1832))
```

sen sijaan että se jatkuvasti kirjoittaa ja uudelleenparsii:

```text
/home/lars/project/src/server/foo.go
```

Tämä muistuttaisi enemmän käyttöjärjestelmän sisäistä rajapintaa kuin terminaalia.

---

# Tästä seuraisi valtava etu: yksi ympäristö

Nykyinen agentti joutuu oppimaan:

```text
GNU grep
BSD grep
BusyBox grep

GNU sed
BSD sed

bash
dash
ksh
PowerShell
cmd.exe

Linux
macOS
FreeBSD
OpenBSD
Windows
```

ja niiden yhdistelmät.

AI-ABI:ssa voisi olla:

```text
search_text()
```

jonka semantiikka olisi **täsmälleen sama kaikkialla**.

Linux-toteutus voisi sisäisesti käyttää mitä haluaa. Windows-toteutus jotain muuta.

AI ei tietäisi eikä välittäisi.

Täsmälleen kuten C-ohjelma ei normaalisti välitä siitä, miten kernel toteuttaa levyn välimuistin.

---

## Se mahdollistaisi myös optimoinnin kokonaan uudella tasolla

Kuvitellaan, että AI pyytää peräkkäin:

```text
files.list(...)
files.filter(...)
text.search(...)
files.read(...)
json.parse(...)
table.aggregate(...)
```

Nykyisin nämä voivat tarkoittaa kuutta prosessia, kuutta datan serialisointia ja valtavaa määrää tekstin kuljettamista.

AI-runtimessa optimizer voisi nähdä koko operaatiograafin:

\[
LIST
\rightarrow FILTER
\rightarrow SEARCH
\rightarrow READ
\rightarrow PARSE
\rightarrow AGGREGATE
\]

ja päättää:

> Teen kaiken yhdellä streaming-passilla.

Vähän samalla tavoin kuin SQL-tietokanta ei kirjaimellisesti toteuta kyselyä käyttäjän kirjoittamassa järjestyksessä vaan tekee query planin.

Tämä voisi olla erittäin paljon tehokkaampi.

---

# Ja tämä liittyy suoraan edelliseen ajatukseemme

Nyt hahmottuu itse asiassa kolme tasoa:

### 1. AI:n reasoning-taso

\[
\text{latentti korkeaulotteinen representaatio}
\]

Täällä päätetään **mitä pitäisi tehdä**.

### 2. AI-native operation layer

Esimerkiksi:

```text
SEARCH
PARSE
FETCH
JOIN
PATCH
COMPILE
SOLVE
AGGREGATE
```

Tämä olisi se ehdottamasi standardi "BusyBox".

Mutta binäärisenä/tyypitettynä IR:nä, ei komentoriviohjelmina.

### 3. Deterministinen executor

Tämä toteuttaa operaatiot:

\[
AI\ IR
\rightarrow
CPU/GPU/kernel/network/filesystem
\]

Ja tässä vasta tulee vastaan fyysinen kone.

---

Minusta sen arkkitehtuuri voisi näyttää tältä:

```text
                    Natural language
                          │
                          ▼
                   ┌───────────────┐
                   │      LLM      │
                   │   reasoning   │
                   └───────┬───────┘
                           │
                     AI operation IR
                           │
            ┌──────────────┴──────────────┐
            │                             │
            ▼                             ▼
     deterministic                  human audit
        runtime                       renderer
            │                             │
     ┌──────┼──────┐             Python / Rust /
     │      │      │             shell / explanation
     ▼      ▼      ▼
    FS     NET    CPU/GPU
```

Tällöin esimerkiksi `sed`, `awk`, `grep`, `curl`, `jq`, `find`, `sort`, `uniq`, `xargs` eivät välttämättä enää olisi **ohjelmia**.

Ne olisivat korkeintaan tuttuja ihmisten nimiä joukolle AI-runtimen primitivejä.

---

Ja tässä olen kanssasi olennaisesti samaa mieltä: **Codexin kaltainen agentti, joka kirjoittaa shell-komennon, lukee stdoutin, kirjoittaa seuraavan shell-komennon ja lukee taas stdoutin, muistuttaa ihmistä näppäilemässä VT100-päätettä.**

Se toimii, koska valtava nykyinen tietokone-ekosysteemi on rakennettu sille rajapinnalle. Mutta ei ole erityistä syytä olettaa, että se olisi hyvä lopullinen rajapinta koneälylle.

Todennäköinen pitkäaikainen ratkaisu olisi mielestäni juuri **AI-native syscall/API + AI-native IR**, jossa shell säilyy lähinnä ihmisten debuggaus- ja yhteensopivuusrajapintana. Silloin AI voisi pyytää "etsi, transformoi, käännä ja testaa tämä riippuvuusgraafi" yhtenä rakenteisena laskentatehtävänä sen sijaan, että se suorittaa 40 kappaletta `grep | sed | awk | xargs` -komentoja.

## Käyttäjä

Jos ajatellaan tietokoneella suoritettavaa tehtävää, niin ensimmäinen asia on tiedosto (DOS käyttis) ja toinen on funktiot (unix pipes). Tiedosto oliona joka on "tekstiä" tai "dataa" on huvittava koska siinä on aina bittejä joku määrä tietyssä järjestyksessä. Apple ja kännököiden Android totettivat erään vanhan ideani, niissä ei ole kuva-tiedostoja, vaan kuvia. Niissä ei ole viesti-tiedostoja vaan viestejä. Normaali käyttäjä ei tarvitse enää tiedostoja ollenkaan kännykässä vaikka se on täysivereinen Unix-tietokone. Unixin ja Plan9 idea oli "kaikki mahdollinen esitetään tiedostona" - mutta paras käyttökokemus nyt on "mikään ei ole tiedosto". Sen sijaan että on xslx tiedostoja, on laskentataulukoita. Koko idean voisi jotenkin rakentaa uudelleen; on "tietoja", "dataa", "kuvia" yms, mutta ei tiedostoja jotka ovat "kansiossa". Jos AI:lla olisi suora pääsy prosessorin käskykantaan ja kaikkiin apulaitteisiin, miten se tallettaisi tiedon. Tiedosto ei välttämättä ole edes staattinen paketti, se voi muuttaa kokoaan, kaikkia tunnuksiaan, yms vaikka koko ajan.

## Avustaja

Kyllä. Tässä päädytään jo paljon syvemmälle kuin siihen, pitäisikö AI:lle tehdä parempi shell. **Itse tiedosto voi olla väärä perusabstraktio.**

Fyysisesti tallennuslaite tarvitsee edelleen jotain hyvin alkeellista:

\[
\text{osoite} \rightarrow \text{bitit}
\]

SSD:llä lopulta on sivuja, blokkeja, flash-soluja jne. Mutta ei ole mitään syytä, että tämän pitäisi näkyä ylemmälle järjestelmälle muodossa

```text
/home/lars/Documents/laskelma.xlsx
```

Se on vain yksi historiallinen tapa nimetä bittijoukko.

Ajattelisin AI:lle tarkoitetun järjestelmän pikemminkin näin:

\[
\boxed{\text{objektit + suhteet + versiot + operaatiot}}
\]

ei:

\[
\boxed{\text{tiedostot + hakemistot + ohjelmat}}
\]

### Esimerkiksi kuva ei olisi tiedosto

Nykyinen Unix-ajatus:

```text
/home/user/photos/IMG_4821.jpg
```

ja järjestelmä tietää oikeastaan vain:

> tässä on 4 287 194 tavua.

Jokin sovellus tulkitsee tavut JPEG-kuvaksi.

Toisenlaisessa järjestelmässä perusolio olisi:

```text
Image #8F23
    pixels
    width
    height
    capture_time
    camera
    location
    people
    edits
    relationships
```

Ja esimerkiksi:

```text
Image #8F23
    belongs_to Album #91
    depicts Person #17
    derived_from Image #7A01
```

JPEG olisi vain **mahdollinen serialisointimuoto**, kun kuva pitää esimerkiksi lähettää ulkopuoliselle järjestelmälle.

Sama ero kuin:

> kuva

vs.

> tiedosto, jonka sisällä sattuu olemaan kuvan representaatio.

Tuo on perustavanlaatuinen ero.

---

## XLSX on erityisen hyvä esimerkki

Kun sanomme nykyään:

> "Excel-tiedosto"

siinä sekoitetaan vähintään kolme täysin eri asiaa.

1. **Laskentataulukko käsitteenä**
2. **sen tämänhetkinen tila**
3. **XLSX-serialisointi levylle**

AI-järjestelmässä voisi olla yksinkertaisesti:

```text
Spreadsheet #42
```

jonka sisäinen rakenne olisi:

```text
Workbook
 ├─ Sheet: Users
 │    ├─ Table
 │    ├─ cells
 │    └─ formulas
 └─ Sheet: Billing
      ├─ Table
      ├─ formulas
      └─ chart
```

Kun ihminen pyytää:

> "lähetä tämä sihteerille Excelinä"

vasta silloin tehdään:

\[
Spreadsheet\ #42
\rightarrow
XLSX\ serializer
\rightarrow
\text{byte stream}
\]

XLSX ei siis olisi alkuperäinen objekti vaan **exporttiformaatti**.

Täsmälleen samoin:

\[
Image \rightarrow JPEG
\]

\[
Document \rightarrow PDF
\]

\[
Audio \rightarrow FLAC
\]

\[
Table \rightarrow CSV
\]

Tiedosto olisi lähinnä **kuljetuspakkaus**.

---

# Mutta nostamasi muuttuvuus tekee tästä vielä kiinnostavamman

Sanoit olennaisen asian:

> tiedon ei tarvitse olla staattinen paketti.

Juuri näin.

Me olemme tottuneet ajattelemaan:

```text
foo.xlsx
```

jonka sisällä olevat bitit muuttuvat.

AI-native järjestelmässä tätä ei ehkä pitäisi mallintaa noin ollenkaan.

Parempi malli voisi olla:

```text
Object ID = 137
```

ja sillä on tiloja:

\[
S_0 \rightarrow S_1 \rightarrow S_2 \rightarrow S_3
\]

Esimerkiksi laskentataulukko:

```text
Spreadsheet #137

version 821:
    ...

version 822:
    cell B17 changed

version 823:
    row 91 added
```

"Spreadsheet #137" pysyy samana käsitteellisenä oliona.

Mutta sen sisältö ei ole yksi tietty tavujono.

Tämä muistuttaa tietokantojen **MVCC:tä** ja persistenttejä tietorakenteita paljon enemmän kuin tavallista tiedostojärjestelmää.

---

## Itse asiassa tallentaisin datan suurelta osin muuttumattomana

Tämä kuulostaa ensin ristiriidalta, koska sanoit että objekti muuttuu jatkuvasti.

Ratkaisu on:

**objektin identiteetti muuttuu, mutta sen vanhaa dataa ei tarvitse muuttaa.**

Esimerkiksi:

```text
Object 137
    current state -> State ABC928
```

Muokataan jotain:

```text
State ABC928
      │
      └── change B17
              │
              ▼
         State D712F0
```

Nyt:

```text
Object 137
    current state -> State D712F0
```

ABC928 jää edelleen olemassa olevaksi versioksi.

Jos käytetään content-addressingia:

\[
ID = H(\text{content})
\]

sisältöpalasen tunniste voidaan johtaa itse datasta.

Silloin sama data tarvitsee tallentaa vain kerran.

Tämä johtaa luonnostaan **Merkle-DAG:n** tapaiseen rakenteeseen:

```text
Spreadsheet
     │
     ├── metadata
     │
     ├── Sheet A
     │      ├── block X
     │      └── block Y
     │
     └── Sheet B
            ├── block Z
            └── block Q
```

Jos muutetaan yksi solu Sheet A:ssa, ei tarvitse kirjoittaa koko "tiedostoa" uudelleen.

Syntyy vain muutama uusi solmu:

```text
old:
        A
       / \
      X   Y

new:
        A'
       /  \
      X    Y'
```

X voidaan jakaa vanhan ja uuden version välillä.

Tämä on erittäin tehokas rakenne AI-järjestelmälle.

---

# Vielä radikaalimpi muutos: hakemistopuu pois

Nykyinen:

```text
/
└── home
    └── lars
        └── projects
            └── ocpp
                └── billing
                    └── august.xlsx
```

on käytännössä tapa pakottaa **yksi hierarkia** tiedon päälle.

Mutta tieto ei luonnostaan muodosta puuta.

Esimerkiksi yksi lataustapahtuma voi kuulua yhtä aikaa:

- käyttäjälle Lars
- latausasemalle #7
- elokuulle 2026
- yritykselle X
- laskulle #831
- OCPP-transaktiolle #A721.

Mihin "kansioon" se kuuluu?

Ei mihinkään yksittäiseen.

Luonnollisempi rakenne on graafi:

```text
             User Lars
                 │
                 │ used
                 ▼
Charger #7 ── Transaction #A721 ── Month 2026-08
                 │
                 │ billed_as
                 ▼
             Invoice #831
```

Sitten kysymys

> "näytä Larsin elokuussa käyttämä sähkö"

on graafikysely eikä hakemistopolun etsimistä.

Tämä on paljon lähempänä sitä, miten **tieto itse asiassa liittyy toiseen tietoon**.

---

# AI:n kannalta tämä olisi erityisen voimakas

AI ei ehkä koskaan pyytäisi:

```bash
find / -name '*.xlsx' | grep 2026
```

vaan:

```text
QUERY
    type = Spreadsheet
    related_to = Billing
    period = 2026-08
```

Tai vielä parempi: AI:n sisäinen runtime voisi tehdä tämän suoraan ilman query-kieltä.

Paluu olisi joukko olioviitteitä:

```text
SpreadsheetRef(137)
SpreadsheetRef(812)
```

Ei tiedostonimiä.

---

## Myös ohjelma lakkaisi olemasta tiedostona

Tässä tulee erittäin kiinnostava symmetria.

Nykyisin:

```text
program.go
database.db
picture.jpg
config.yaml
```

ovat kaikki tiedostoja.

AI-järjestelmässä voisi olla:

```text
ExecutableGraph #44
Dataset #71
Image #22
Configuration #18
```

Ja niiden suhteet:

```text
ExecutableGraph #44
    READS Dataset #71
    PRODUCES Report #882
    USES Configuration #18
```

Eli **data ja ohjelma muuttuvat saman tietograafin eri tyyppisiksi solmuiksi**.

Tämä alkaa jo muistuttaa enemmän Lisp-/Smalltalk-tyyppistä objektimaailmaa kuin Unixia.

---

# Silloin prosessikin voidaan kyseenalaistaa

Jos tiedosto on historiallinen abstraktio, myös:

```text
process
stdin
stdout
current working directory
environment variables
```

ovat pitkälti Unix-maailman abstraktioita.

AI voisi pyytää:

```text
Transform(
    source = TransactionSet #712,
    operation = BillingAggregation,
    destination = Report #882
)
```

Runtime toteuttaisi sen missä tahansa.

Ei välttämättä olisi mitään pitkäikäistä "prosessia".

Vain:

\[
\text{objekti}
\rightarrow
\text{operaatio}
\rightarrow
\text{uusi objekti}
\]

---

# Entä jos AI pääsisi suoraan laitteisiin?

Silloin tekisin myös laitteista objekteja.

Ei:

```text
/dev/nvme0n1
/dev/video0
/dev/ttyUSB0
```

vaan esimerkiksi:

```text
Camera #12
Display #3
StoragePool #8
NetworkInterface #5
TemperatureSensor #71
EVCharger #22
```

ja niiden capabilities:

```text
Camera #12:
    CaptureImage
    CaptureVideo
    SetExposure

EVCharger #22:
    ReadMeter
    SetCurrentLimit
    StopCharging
```

AI ei kirjoittaisi bittejä rekisteriin, ellei siihen olisi aivan erityistä syytä.

Se käyttäisi **semanttista laite-ABI:a**.

Alimmalla tasolla driver muuttaa:

\[
SetCurrentLimit(10A)
\]

esimerkiksi USB-/CAN-/PCIe-/MMIO-operaatioiksi.

Sama periaate kuin edellisessä keskustelussamme AI-native BusyBoxista, mutta vietynä koko tietokoneeseen.

---

## Tästä voisi muodostua hyvin puhdas malli

Minä hahmottaisin sen neljän käsitteen varaan:

\[
\boxed{\text{Object}}
\]

jokin asia: kuva, henkilö, taulukko, lataustapahtuma, ohjelma, anturi.

\[
\boxed{\text{Relation}}
\]

miten asiat liittyvät toisiinsa.

\[
\boxed{\text{State}}
\]

objektin tila tietyllä hetkellä.

\[
\boxed{\text{Operation}}
\]

deterministinen tai AI:n ohjaama muunnos:

\[
(State,\ Operation) \rightarrow State'
\]

Fyysinen tallennus jäisi piiloon:

```text
                 AI
                  │
                  ▼
       Object / Relation / Operation
                  │
                  ▼
       persistent object runtime
                  │
         ┌────────┴────────┐
         ▼                 ▼
       RAM/cache       persistent store
                            │
                            ▼
                     blocks / flash
```

Ja huomaa, mitä tästä puuttuu kokonaan:

```text
filename
extension
folder
PATH
open()
close()
seek()
stdin
stdout
```

Ne eivät ole välttämättömiä käsitteitä.

---

Minusta tässä on yksi erityisen tärkeä johtopäätös: **"save" voisi kadota kokonaan.**

Nykyisessä tiedostomaailmassa ihminen:

1. avaa tiedoston,
2. muuttaa muistissa olevaa kopiota,
3. painaa Save,
4. bittijono kirjoitetaan takaisin tiedostoon.

Objektijärjestelmässä jokainen hyväksytty tilamuutos voisi olla automaattisesti persistentti:

\[
S_5 \rightarrow S_6
\]

Ei ole mitään erillistä "tallenna tiedosto" -operaatiota.

Samalla saadaan historia ilmaiseksi:

\[
S_1\rightarrow S_2\rightarrow S_3\rightarrow \ldots\rightarrow S_n
\]

Undo ei enää tarkoita, että sovellus itse yrittää ylläpitää undo-pinoa. Järjestelmä yksinkertaisesti osoittaa aiempaan tilaan.

Siinä mielessä **kännyköiden "ei ole tiedostoja, on kuvia ja viestejä" on vain ensimmäinen hyvin pinnallinen askel**. Alla on edelleen Unix-tiedostojärjestelmä. Radikaalimpi versio olisi rakentaa koko tietokone niin, ettei *sovelluskaan* ensisijaisesti näe tiedostoja, vaan persistenttejä, tyypitettyjä ja versioituja objekteja.

Ja juuri AI:n myötä tällainen arkkitehtuuri alkaa mielestäni olla paljon järkevämpi kuin silloin, kun koko koneen käyttöliittymän piti olla ihmiselle ymmärrettävä `ls`, `cd`, `cp`, `foo.txt` -maailma.

## Käyttäjä

Onko jossain Lisp tms. kielessä Object, Relation, State, Operation malli?

## Avustaja

Täsmälleen tuota nelikkoa **Object–Relation–State–Operation** en tunne vakiintuneena Lisp-kielimallina, mutta pari järjestelmää tulee hyvin lähelle. Erityisesti **Clojure + Datomic** osuu yllättävän tarkasti siihen, mitä kuvasit.

## Avustaja

Kyllä. **Lähimmäksi kuvaamaamme mallia osuu mielestäni Clojure + Datomic**, vaikka Datomic on tietokanta eikä varsinainen käyttöjärjestelmä tai ohjelmointikieli.

Datomicin perusyksikkö on `datom`:

\[
\boxed{E,\ A,\ V,\ Tx}
\]

eli:

- **E = Entity** — tunnistettava olio
- **A = Attribute** — ominaisuus tai relaatio
- **V = Value** — arvo tai viittaus toiseen entityyn
- **Tx = Transaction** — tapahtuma, jossa fakta tuli voimaan tai lakkasi olemasta voimassa.

Datomicin dokumentaatio kuvaa tietokannan nimenomaan joukkona muuttumattomia atomisia faktoja. Datomia ei "päivitetä paikallaan", vaan uusi transaktio lisää informaatiota; koko historia voidaan säilyttää ja tietokantaa voidaan tarkastella tiettynä ajanhetkenä. 

Tämä osuu hämmästyttävän hyvin meidän nelikkoomme:

| Meidän käsite | Datomic |
|---|---|
| **Object** | Entity `E` |
| **Relation** | Attribute `A`, erityisesti `ref` toiseen entityyn |
| **State** | database value tiettynä ajanhetkenä `t` |
| **Operation** | Transaction / transaction function |

### Esimerkiksi ei "kuvatiedostoa"

Voitaisiin sanoa konseptuaalisesti:

```clojure
{:db/id 1001
 :object/type :image
 :image/width 4032
 :image/height 3024
 :image/camera 431
 :image/depicts 927}
```

Tässä `431` ja `927` voivat olla viittauksia muihin entityihin. Datomicin `ref`-attribuutit tekevät juuri tällaisia entity→entity-suhteita, ja graafia voidaan navigoida niiden kautta. 

Ei tarvitse olla:

```text
/DCIM/Camera/IMG_2047.JPG
```

järjestelmän varsinaisena identiteettinä.

JPEG voisi olla vain yksi representaatio entitystä 1001:

\[
Image(1001) \xrightarrow{\mathrm{serialize}} JPEG
\]

---

## Vielä mielenkiintoisempi on State

Perinteisessä OO:ssa ajateltaisiin:

```text
object.temperature = 23
```

ja myöhemmin:

```text
object.temperature = 24
```

Vanha 23 häviää.

Datomicin maailmassa ajatus on lähempänä:

\[
(E,\ temperature,\ 23,\ Tx_{100})
\]

sitten:

\[
(E,\ temperature,\ 24,\ Tx_{137})
\]

ja järjestelmä tietää sekä vanhan että uuden tilanteen.

Entity itsessään on näkymä entityn tietoihin **tietyllä database-ajankohdalla**. Entity-arvot ovat immuuttisia. 

Voit siis käsitteellisesti kysyä:

```text
temperature(entity, NOW)
```

tai:

```text
temperature(entity, yesterday)
```

Tämä vastaa todella hyvin edellisessä viestissä hahmottelemaamme:

\[
S_0\rightarrow S_1\rightarrow S_2\rightarrow S_3
\]

sen sijaan että olisi yksi muuttuva tiedosto.

---

## CLOSissa sama löytyy, mutta eri tavalla

Perinteisessä **Common Lisp Object Systemissä (CLOS)** voidaan myös tehdä lähes koko nelikko:

**Object**

```lisp
(defclass person ()
  ((name ...)
   (car ...)))
```

**State**

olion slotit.

**Operation**

```lisp
(defgeneric drive (person car))
(defmethod drive ((p person) (c car))
  ...)
```

CLOSissa operaatio on kiinnostavasti **generic function**, ei välttämättä jonkin objektin sisään suljettu `object.method()`. CLOS:n metaobject protocolissa luokat, metodit, generic functionit jne. ovat lisäksi itse first-class-olioita. 

Mutta **Relation** on siinä heikompi kohta.

Jos:

```lisp
(person-car lars)
```

palauttaa auton, tämä on käytännössä olion slotissa oleva viittaus.

CLOS ei itsessään sano:

\[
\operatorname{owns}(Lars,Car17)
\]

sellaisena ensimmäisen luokan relaationa, jota voisi kysellä molempiin suuntiin tai johon voisi liittää historiaa.

Datomic taas tekee juuri tämän luonnollisesti.

---

## Prolog/Datalog on toisesta suunnasta lähellä

Prologissa luonnollinen maailma olisi:

```prolog
person(lars).
car(car17).
owns(lars, car17).
located_at(car17, pori).
```

Tässä **relaatio on ensisijainen käsite**.

Ei tarvitse olla:

```text
Lars {
    cars: [...]
}
```

vaan:

\[
owns(Lars,Car17)
\]

on itsenäinen fakta.

Datomic käyttää kyselyihin juuri **Datalogia**, joten siinä yhdistyvät kiinnostavasti nämä kaksi ideaa: entityt ja relaatioihin perustuva logiikkakysely. 

---

# Datomicissa on yksi erityisen kiinnostava piirre ajatuksemme kannalta

**Transaktio on itsekin entity.**

Siis tapahtuma:

> Lars muutti latausaseman virtarajan 16 A → 10 A

ei ole vain näkymätön tietokantaoperaatio.

Sillä voi itsellään olla tietoja:

```text
Transaction #98172
    actor       -> Lars
    device      -> Charger #17
    reason      -> load balancing
    timestamp   -> ...
```

Datomic dokumentoi tämän nimenomaan niin, että transaktioille voi lisätä provenancea, käyttäjän, tarkoituksen jne. 

Tämä vie mallia kohti:

\[
\boxed{Object}
\]

\[
\boxed{Relation}
\]

\[
\boxed{State}
\]

\[
\boxed{Operation/Event}
\]

missä **kaikki neljä ovat itse osa samaa tietomaailmaa**.

---

### Ja tästä voisi tehdä kokonaisen käyttöjärjestelmän

Tämä ei enää ole Datomic sellaisenaan, mutta kuvittele:

```text
Entity 17    : Image
Entity 21    : Person
Entity 47    : EVCharger
Entity 89    : Spreadsheet
Entity 104   : Program
Entity 712   : NetworkConnection
```

ja:

```text
17 --depicts--> 21
47 --located-at--> 381
89 --derived-from--> 900
104 --reads--> 89
```

Kaikki muutos olisi transaktio:

\[
DB_t + Tx \rightarrow DB_{t+1}
\]

ja jokainen \(DB_t\) olisi immuuttinen kokonainen maailmantila.

Ei tarvittaisi perustavalla tasolla:

```text
file
directory
pathname
save
open
close
```

vaan paljon lähempänä olisi:

\[
\boxed{\text{Entity + Fact + Relation + Time + Transformation}}
\]

**Datomicin EAVT on siksi ehkä paras olemassa oleva konkreettinen esimerkki siitä suunnasta, johon olemme tässä päätyneet.** Se ei ole AI-käyttöjärjestelmä, mutta sen informaatiomalli olisi mielestäni erittäin hyvä lähtökohta sellaiselle.

Ja siinä on hauska yhteys Unixiin: Unix sanoi **"everything is a file"**. Tällainen järjestelmä voisi sanoa pikemminkin **"everything is an entity, and everything known about entities consists of facts and relations over time."**

## Käyttäjä

Oikein hyvä. Haluan analysoida "mitä tietokoneella tehdään" - sen  sijaan että miten "tietokone tekee".  AI on muuttanut koko viitekehyksen. AIkasemmin notepad, selain, yms ovat olleet "interaktiivinen kuva". Kuva kirjaston kirjasta (wikipedia), kuva paperista johon kirjoitetaan (notepad) ja kuva paperista johon piirretään (cad). Nyt kielimalli on tehnyt tietokoneesta "tietokoneen". Mielestäni käyttö on tulevaisuudessa ensisijaisesti "kysymys" -> "vastaus" ja  toissijaisesti "käsky" -> "suoritus". Voi kysyä tietoa tai käskeä piirtämään kuvan tai tekemään ohjelman.

## Avustaja

Niin. Tässä vaihtuu oikeastaan koko tietojenkäsittelyn perusmetafora.

Aikaisempi henkilökohtainen tietokone oli ennen kaikkea **simulaatio työpöydästä**. Näytöllä esitettiin asioita, joita ihminen käsitteli ennen fyysisesti:

- tekstinkäsittely = paperi + kirjoituskone
- CAD = piirustuslauta
- selain = kirja/lehti/kirjasto
- sähköposti = kirje
- tiedostonhallinta = arkistokaappi
- laskentataulukko = ruutupaperi + laskin.

GUI teki näistä interaktiivisia kuvia. Hiiri oli pitkälti tapa osoittaa näiden kuvien osia.

AI muuttaa perusyksikön. Käyttäjän ei enää tarvitse ensisijaisesti **manipuloida representaatiota**, vaan hän voi ilmaista tarkoituksen.

Vanha malli:

\[
\boxed{\text{ihminen manipuloi käyttöliittymää}}
\rightarrow
\boxed{\text{sovellus}}
\rightarrow
\boxed{\text{data}}
\]

Uusi malli voisi olla:

\[
\boxed{\text{intentio}}
\rightarrow
\boxed{\text{tulkinta}}
\rightarrow
\boxed{\text{operaatio tietoon}}
\]

Ja intentioita on mielestäni kaksi hyvin perustavaa laatua.

## 1. Kysymys → vastaus

Tämä on:

\[
Q(S) \rightarrow A
\]

missä \(S\) on järjestelmän tuntema maailma tai tietotila.

Esimerkiksi:

> Paljonko sähköä latausasema 7 käytti elokuussa?

Nykyisessä tietokoneessa käyttäjän täytyy ehkä:

1. avata tietokanta,
2. löytää oikea taulu,
3. tehdä SQL-kysely,
4. viedä CSV,
5. avata Excel,
6. summata sarake.

Mutta käyttäjän varsinainen tehtävä ei ollut mikään näistä.

Tehtävä oli:

\[
\boxed{\text{selvitä arvo}}
\]

Kaikki muu oli tietokoneen implementaatiota.

AI tekee ensimmäistä kertaa mahdolliseksi sen, että käyttöliittymä vastaa suoraan tehtävän semantiikkaa.

---

## 2. Käsky → tilanmuutos

Toinen perusmuoto on:

\[
C(S) \rightarrow S'
\]

Esimerkiksi:

> Rajoita tämän latausaseman virta 10 ampeeriin.

Tai:

> Piirrä tästä osasta valmistuskuva.

Tai:

> Tee tästä kuukausiraportti.

Tai:

> Korjaa ohjelmasta tämä virhe.

Käyttäjä ei oikeastaan halua käyttää CAD-ohjelmaa, Exceliä tai editoria.

Hän haluaa:

\[
\boxed{\text{muuttaa maailman tilaa}}
\]

tai luoda uuden objektin.

Tämä sopii erittäin hyvin siihen aiempaan malliin:

\[
\boxed{Object,\ Relation,\ State,\ Operation}
\]

Kysymys lukee maailmaa:

\[
Query(State) \rightarrow Information
\]

Käsky muuttaa sitä:

\[
Operation(State) \rightarrow State'
\]

---

# Sovellus lakkaa olemasta keskeinen käsite

Tämä on mielestäni yksi suurimmista seurauksista.

Nykyisin ihminen ajattelee:

> Haluan tehdä diagrammin.

Sitten:

> Millä ohjelmalla diagrammeja tehdään?

Sitten avataan Excel, LibreOffice, Python, Matlab tms.

AI-maailmassa käyttäjän pitäisi vain sanoa:

> Piirrä tämän datan virta ajan funktiona ja merkitse poikkeamat.

**"Mitä ohjelmaa käytetään?" on implementaatiokysymys.**

Järjestelmän sisäinen capability voisi olla esimerkiksi:

\[
Plot(Data,\ Specification) \rightarrow Figure
\]

Ei ole olennaista, toteuttaako sen matplotlib, GPU-kirjasto vai kokonaan AI:lle suunniteltu grafiikkamoottori.

Samoin:

> tee PDF

ei tarkoita:

> käynnistä LibreOffice.

Vaan:

\[
Document \rightarrow Render(PDF)
\]

Sovelluksista tulee siis enemmän **kyvykkyyksiä** kuin käyttäjän itse käynnistämiä maailmoja.

---

# Tämä tekee myös "dokumentista" vähemmän tärkeän

Esimerkiksi nykyinen prosessi:

```text
avaa Excel
→ avaa.xlsx
→ etsi taulukko
→ muuta solu
→ tallenna
```

semanttisesti:

\[
Spreadsheet(83).VATRate := 25.5\%
\]

Tai vielä paremmin:

> Päivitä vuoden 2026 laskutuksessa käytetty ALV nykyiseen arvoon.

AI ratkaisee:

- mitä tietoa tarkoitetaan,
- mitä objekteja muutos koskee,
- mitkä riippuvuudet pitää laskea uudelleen.

Ihminen ei tarvitse solukoordinaattia `F37`.

---

## Käyttöliittymän rooli muuttuu

GUI ei välttämättä katoa. Se muuttuu **ensisijaisesta kontrollimekanismista tuloksen tarkasteluvälineeksi**.

Esimerkiksi CAD:ssa:

> Tee tähän 8 mm kiinnitysreikä 20 mm reunasta.

AI tekee muutoksen.

Sen jälkeen GUI näyttää objektin, jotta ihminen voi tarkistaa:

> kyllä, juuri tuohon.

Eli:

\[
\text{kieli} \rightarrow \text{toiminta}
\]

ja kuva toimii palautteena:

\[
\text{toiminta} \rightarrow \text{visuaalinen tarkastus}.
\]

Nykyisessä CAD:ssa järjestys on päinvastainen: ihminen manipuloi kuvaa saadakseen aikaan toiminnan.

---

# "Kysymys" on vielä paljon laajempi käsite kuin nykyinen haku

Google-tyyppinen järjestelmä vastaa suunnilleen:

\[
query \rightarrow documents
\]

AI-järjestelmän luonnollinen tehtävä on:

\[
query \rightarrow answer
\]

Tämä on valtava muutos.

Jos kysyt:

> Mikä näistä kolmesta moottorista sopii tähän parhaiten?

et oikeastaan halua 25 verkkosivua.

Haluat päätelmän:

\[
\text{requirements}
+
\text{motor data}
+
\text{engineering reasoning}
\rightarrow
\boxed{\text{selection}}
\]

ja mielellään perustelut.

Sama pätee paikalliseen tietoon:

> Mitkä asiakkaat eivät ole maksaneet?

Vanha tietokone:

\[
\text{näytä tietokanta}
\]

AI-tietokone:

\[
\boxed{\text{vastaa kysymykseen}}
\]

---

## Kolmas vuorovaikutusmuoto tulee mielestäni näiden lisäksi: tavoite

Kysymys:

> Mikä on X?

Käsky:

> Tee X.

Mutta AI mahdollistaa myös:

> Huolehdi, että X pysyy kunnossa.

Esimerkiksi:

> Pidä latausjärjestelmän kokonaisvirta alle 63 A:ssa mutta anna autoille mahdollisimman paljon lataustehoa.

Tämä ei ole yksi käsky.

Se on:

\[
\boxed{\text{Goal}}
\]

jonka perusteella järjestelmä suorittaa ajan yli operaatiota:

\[
S_t
\rightarrow
Action_t
\rightarrow
S_{t+1}
\]

ja jälleen:

\[
S_{t+1}
\rightarrow
Action_{t+1}
\]

Tässä tietokone muuttuu suorastaan säätöjärjestelmäksi.

---

Siksi koko henkilökohtaisen tietokoneen käyttömallin voisi ehkä tiivistää näin:

### Vanha

\[
\boxed{\text{Application}}
\rightarrow
\boxed{\text{Document}}
\rightarrow
\boxed{\text{GUI operations}}
\]

### AI-native

\[
\boxed{\text{Question}}
\rightarrow
\boxed{\text{Answer}}
\]

\[
\boxed{\text{Command}}
\rightarrow
\boxed{\text{Operation}}
\]

\[
\boxed{\text{Goal}}
\rightarrow
\boxed{\text{ongoing control}}
\]

ja niiden alla yhteinen maailma:

\[
\boxed{\text{Objects + Relations + State}}
\]

Minusta juuri tässä merkityksessä lauseesi **"kielimalli on tehnyt tietokoneesta tietokoneen"** on osuva. Aikaisempi PC oli suurelta osin erittäin hienostunut elektroninen työpöytä, jossa ihminen edelleen suoritti tiedollisen työn. AI:n myötä kone voi ensimmäistä kertaa ottaa vastaan itse **tehtävän**, eikä pelkästään käyttäjän määräämää sarjaa mekaanisia operaatioita.

Seuraava kiinnostava askel olisi mielestäni yrittää määritellä aivan tyhjästä: **mitkä ovat kaikki tietokoneella tehtävän työn perusoperaatiot**, jos unohdetaan kokonaan tiedostot, sovellukset, ikkunat, hiiri, shell ja nykyiset käyttöjärjestelmät. Siitä voisi hyvinkin saada yllättävän pienen joukon primitiivejä.

## Käyttäjä

HAL:n käyttöliittymä oli pelkkä puhe. Se voi olla hyvä, mutta esimerkiksi tämän keskustelun käyminen olisi siinä mielessä outoa että monimutkaisen asian selittäminen on ihmiselle vaikeaa. Puheesta tulee Zizekin puhetta, varmasti älykästä mutta epäselvää toistoa ja ähinää verrattuna kirjoitettuun selvään tekstiin.

## Avustaja

Täsmälleen. **Puhe on hyvä käyttöliittymä intentiolle, mutta huono käyttöliittymä monimutkaiselle rakenteelle.** Siksi en usko, että AI-tietokoneen lopullinen käyttöliittymä olisi HAL-tyyppinen pelkkä puhe.

Puheen ongelma on, että se on lähes kokonaan **lineaarinen ja katoava**. Kun sanon kymmenen minuutin selityksen, kuulijan pitää pitää rakenne työmuistissa:

\[
A \rightarrow B \rightarrow C \rightarrow D
\]

Jos kohdassa \(D\) huomaa ongelman kohdassa \(B\), täytyy alkaa selittää takaisinpäin:

> "Siis siinä aiemmin, kun sanoin että..."

Kirjoitettu teksti toimii aivan eri tavalla. Se on ihmisen **ulkoinen työmuisti**. Voit silmäillä tämän keskustelun neljä kappaletta taaksepäin, verrata kahta kohtaa ja palata takaisin muutamassa sekunnissa.

Tässä mielessä kirjoitus ei ole vain puheen tallenne. Se on erilainen kognitiivinen väline.

Esimerkiksi:

\[
Object + Relation + State + Operation
\]

on paljon helpompi ymmärtää silmillä yhtenä rakenteena kuin kuulla:

> "object plus relation plus state plus operation..."

Sama pätee taulukkoon:

| asia | vanha malli | AI-malli |
|---|---|---|
| tieto | tiedosto | objekti |
| käsittely | sovellus | operaatio |
| haku | polku | relaatio/kysely |

Puheena tuollainen taulukko muuttuu helposti pitkäksi jaaritteluksi.

### Puhe sopii erityisen hyvin lyhyisiin intentioihin

Esimerkiksi:

> "Näytä viime kuun lataukset."

> "Laita tähän 8 mm reikä."

> "Mikä tässä ohjelmassa on vikana?"

> "Lue tämä minulle."

Tai tilanteisiin, joissa kädet ja silmät ovat muualla.

Puhe on tällöin erittäin tehokas:

\[
\text{intentio} \rightarrow \text{AI}
\]

Mutta vastauksen ei tarvitse tulla samalla modaliteetilla.

> "Vertaa nämä kolme moottoria."

on erinomainen puhekomento.

Vastaus kannattaa ehkä antaa **taulukkona**, ei lukea ääneen 25 ominaisuutta.

---

## Siksi AI:n luonnollinen käyttöliittymä on mielestäni multimodaalinen

Ei siis:

\[
speech \leftrightarrow speech
\]

vaan:

\[
\boxed{\text{ihminen ilmaisee asian sopivimmalla tavalla}}
\]

ja

\[
\boxed{\text{kone vastaa sopivimmalla representaatiolla}}.
\]

Esimerkiksi:

**Puhe → kuva**

> "Näytä missä laakeri ottaa kuormaa."

→ kone piirtää kuvan.

**Teksti → laskelma**

> "Miksi tässä on 15 % ero?"

→ teksti + yhtälöt + taulukko.

**Kuva → teksti**

> "Mikä tässä hitsauksessa näyttää väärältä?"

→ kone analysoi kuvaa ja merkitsee kohdan.

**Teksti → toiminta**

> "Rajoita latausvirta 10 A:iin."

→ toiminto.

**Puhe → ohjelma**

> "Tee ohjelma joka laskee tästä tietokannasta kuukausilaskutuksen."

→ ohjelma/operation graph.

---

### Kirjoitus säilyttää vielä yhden olennaisen edun: editoinnin

Puheessa sanot:

> "Tee... ei kun siis ota käyttäjät... paitsi nuo kaksi... ja oikeastaan elokuusta..."

Tekstissä voi muodostaa lopullisen täsmällisen käskyn:

> Laske elokuun 2026 energiankulutus käyttäjittäin, jätä käyttäjät 1 ja 2 pois ja ilmoita tulos kWh:na.

Sen voi lukea ennen suoritusta.

Tämä on tärkeää etenkin silloin, kun käsky muuttaa maailmaa.

AI voisi jopa automaattisesti erottaa:

\[
\text{raakaintentio}
\rightarrow
\boxed{\text{täsmennetty operation specification}}
\rightarrow
\text{suoritus}.
\]

Käyttäjä puhuu luonnollisesti ja epätäsmällisesti, mutta ennen merkittävää toimenpidettä kone näyttää siitä tarkan kirjallisen representaation.

---

## Ja kaaviot ovat vielä kolmas kognitiivinen kieli

Tässä keskustelussamme olemme koko ajan päätyneet kaavioihin:

\[
Question \rightarrow Answer
\]

\[
Command \rightarrow Operation
\]

\[
Goal \rightarrow Control
\]

Niitä voisi toki lausua puheena, mutta yhdellä silmäyksellä nähty rakenne on tehokkaampi.

Samoin ohjelmiston arkkitehtuuri:

```text
            AI
             │
       operation graph
             │
     ┌───────┼───────┐
     ▼       ▼       ▼
   data    network   device
```

on informaatiotiheydeltään aivan eri asia kuin saman asian sanallinen kuvaus.

---

Siksi pitäisin tulevaisuuden AI-käyttöliittymän lähtökohtana ehkä tätä:

\[
\boxed{\text{Conversation is primary}}
\]

mutta

\[
\boxed{\text{speech is only one representation of conversation}}.
\]

"Keskustelu tietokoneen kanssa" ei siis tarkoita välttämättä puhumista.

Tämä nykyinen tekstikeskustelu on itse asiassa hyvin vahva käyttöliittymä, koska siinä yhdistyvät:

- luonnollinen kieli,
- pysyvä historia,
- yhtälöt,
- koodi,
- taulukot,
- kuvat,
- linkit,
- mahdollisuus palata aikaisempaan kohtaan.

HAL oli vaikuttava, koska vuonna 1968 jo se, että tietokone **ymmärsi puhetta**, oli radikaalia. Mutta jos suunnittelisi käyttöliittymän nyt tyhjästä, en tekisi HAL:sta pelkkää ääntä. Tekisin siitä ennemmin **jatkuvan keskustelutilan**, jossa voi puhua silloin kun puhe on tehokasta ja käyttää tekstiä, kuvia, kaavioita ja rakenteista dataa silloin kun ne ovat ihmiselle kognitiivisesti parempia.

Ja juuri pitkissä abstrakteissa keskusteluissa, kuten tässä, **kirjoitettu keskustelu on todennäköisesti puhetta parempi päämodaliteetti**.

## Käyttäjä

mitä tälle tekstille tehdään ensin? Tehdäänkö kaikista kielistä joku väli-representaatio vai mitkä ovat vaiheet kysymyksestä eteenpäin.

## Avustaja

Ei yleensä niin, että suomi ensin käännettäisiin johonkin eksplisiittiseen "AI-kieleen" tai englantiin. Paljon lähempänä totuutta on:

\[
\text{teksti}
\rightarrow
\text{tokenit}
\rightarrow
\text{vektorit}
\rightarrow
\text{monikerroksinen latentti representaatio}
\rightarrow
\text{päätös / seuraava token / toiminto}
\]

Vaiheet ovat karkeasti nämä.

### 1. Teksti pilkotaan tokeneiksi

Esimerkiksi lause

> "Mitä tietokoneella tehdään?"

ei mene verkkoon kirjaimina tai sanoina sellaisenaan, vaan tokenizer pilkkoo sen tokeniyksiköiksi.

Ne voivat olla kokonaisia sanoja, sanan osia tai byteihin perustuvia jaksoja. Ajatus on suunnilleen:

```text
"Mitä"
" tieto"
"kone"
"ella"
" tehdään"
"?"
```

Tämä ei ole oikea tokenisaatioesimerkki juuri tästä mallista, vaan havainnollistus.

Jokaisella tokenilla on kokonaislukutunnus:

\[
[18372,921,\ldots]
\]

Tässä vaiheessa `suomi`, `englanti`, `venäjä` jne. eivät ole enää varsinaisesti merkkijonoja vaan tokenisarjoja.

---

### 2. Tokeni muutetaan vektoriksi

Jokaiselle tokenille haetaan embedding:

\[
token_i \rightarrow x_i\in\mathbb{R}^d
\]

missä \(d\) voi olla tuhansia ulottuvuuksia.

Esimerkiksi:

\[
\text{"koira"}\rightarrow
[0.17,-0.83,1.21,\ldots]
\]

Mutta tämä ensimmäinen embedding ei vielä tarkoita yksinkertaisesti:

> "koira = nelijalkainen eläin"

Se on lähtöpiste myöhemmälle laskennalle.

Myös tokenin sijainti tekstissä otetaan huomioon.

---

# 3. Sitten alkaa varsinainen kiinnostava osa

Transformer-kerrokset käsittelevät koko kontekstia.

Ensimmäisten kerrosten jälkeen representaatio voi sisältää paljon paikallista tietoa:

- tämä näyttää suomen kieleltä
- tämä token kuuluu tähän sanaan
- tämä sana liittyy edelliseen sanaan
- tämä rakenne näyttää kysymykseltä.

Syvemmällä syntyy abstraktimpia rakenteita:

- käyttäjä kysyy prosessista
- kohteena on kielimallin tekstinkäsittely
- käyttäjä vertaa kielten välistä representaatiota
- pitäisi erottaa tokenisaatio ja semanttinen representaatio.

Tämä kaikki on numeerisessa muodossa.

Eli kullakin kohdalla on jotakin:

\[
h_i^{(1)}
\rightarrow
h_i^{(2)}
\rightarrow
h_i^{(3)}
\rightarrow \ldots
\rightarrow h_i^{(L)}
\]

missä jokainen \(h\) on suuri vektori.

---

# Onko kaikille kielille yhteinen välikieli?

**Ei eksplisiittisessä mielessä.**

Ei siis ole tiettyä vaihetta:

```text
suomi
↓
Universal AI Language
↓
reasoning
```

tai:

```text
suomi
↓
englanti
↓
reasoning
```

Mutta käytännössä verkon syvemmät representaatiot voivat muuttua **osittain kielestä riippumattomiksi**.

Esimerkiksi:

> koira

> dog

> Hund

> chien

ovat eri tokenijonoja.

Mutta ne kaikki liittyvät hyvin samankaltaiseen käsitteeseen. Koulutuksessa verkolle on hyödyllistä muodostaa representaatiot, joissa niiden merkitykset sijaitsevat jotenkin lähekkäin tai joissa samat myöhemmät laskentapiirit voivat käsitellä niitä.

Voisi ajatella:

\[
\begin{aligned}
\text{koira}&\rightarrow z_{\mathrm{fi}}\\
\text{dog}&\rightarrow z_{\mathrm{en}}\\
\text{Hund}&\rightarrow z_{\mathrm{de}}
\end{aligned}
\]

ja myöhemmässä verkossa:

\[
z_{\mathrm{fi}},z_{\mathrm{en}},z_{\mathrm{de}}
\longrightarrow
\boxed{\text{samankaltainen semanttinen rakenne}}
\]

Ei kuitenkaan välttämättä yhtä yksittäistä vektoria nimeltä `DOG_CONCEPT`.

Representaatio on hajautunut.

---

## Tämä on lähempänä ajatustasi "väli-IR:stä"

Ohjelmointikielissä:

```text
C
 ┐
C++ ─→ LLVM IR → x86
Rust┘
```

Kielimallissa voisi analogisesti sanoa:

```text
Finnish ┐
English ├→ latent semantic computation → output
German  ┘
```

Mutta ero on tärkeä:

**LLVM IR on ihmisen määrittelemä formaali kieli.**

LLM:n latentti representaatio on koulutuksessa itse syntynyt, jatkuva-arvoinen ja hajautunut.

Ei ole spesifikaatiota:

```text
opcode 37 = ownership relation
opcode 81 = logical implication
```

---

# 4. Attention yhdistää asioita toisiinsa

Transformerissa yksi keskeinen operaatio on attention.

Hyvin karkeasti jokainen tekstin kohta voi kysyä:

> mihin muihin kohtiin minun pitäisi juuri nyt kiinnittää huomiota?

Matemaattisesti:

\[
Attention(Q,K,V)
=
softmax\left(\frac{QK^T}{\sqrt d}\right)V
\]

Jos kysyt:

> "Kun sanoit aiemmin että tiedosto on väärä abstraktio, mitä tarkoitit?"

verkko voi muodostaa voimakkaita yhteyksiä:

```text
"tiedosto"
        ↕
aikaisempi keskustelu tiedostoista
        ↕
"abstraktio"
```

Ei tarvita eksplisiittistä hakukomentoa siihen nykyisen kontekstin sisällä.

---

# 5. Jokainen kerros muokkaa yhteistä "residual streamia"

Tätä voi ajatella eräänlaisena työtilana.

Kuvitteellisesti:

```text
alku:
  tokenit + sijainnit

kerros 5:
  sanamuoto + syntaksi

kerros 20:
  käsitteet + suhteet

kerros 40:
  kysymyksen tarkoitus + mahdollinen vastausrakenne

...
```

Tämä on vain havainnollistus; todellisessa verkossa asiat eivät jakaudu näin siististi kerroksiin.

Yhdessä kerroksessa voi samanaikaisesti käsitellä:

- kielioppia
- referenssejä
- matematiikkaa
- maailman tietoa
- vastaustyyliä.

---

# 6. Lopussa ei tule "valmista lausetta"

Transformer tuottaa jakauman seuraavalle tokenille:

\[
P(t_{next}\mid context)
\]

Esimerkiksi:

```text
"Ei"       0.31
"Kyllä"    0.18
"Ensimmäinen" 0.09
...
```

Valitaan seuraava token, lisätään se kontekstiin ja sama laskenta jatkuu.

Siis:

\[
context
\rightarrow token_1
\]

\[
context+token_1
\rightarrow token_2
\]

\[
context+token_1+token_2
\rightarrow token_3
\]

jne.

Tästä syntyy vastaus.

---

## Mutta reasoning-mallissa tähän tulee tärkeä lisä

Kaikkea laskentaa ei kannata ajatella pelkkänä:

> "ennusta seuraava sana."

Mallin voidaan antaa käyttää enemmän laskentaa ennen lopullista vastausta ja muodostaa sisäisiä välitiloja ongelman ratkaisemiseksi.

Konseptuaalisesti:

\[
\text{kysymys}
\rightarrow
\boxed{\text{latentti ongelmarepresentaatio}}
\rightarrow
\boxed{\text{välipäättely}}
\rightarrow
\boxed{\text{ratkaisun representaatio}}
\rightarrow
\text{vastaustokenit}
\]

Tämä on lähempänä sitä "AI-native laskentaa", josta olemme puhuneet.

---

# Ja jos tarvitaan ulkopuolista toimintoa

Esimerkiksi:

> laske tämä tarkasti

tai

> etsi tämä verkosta

malli voi päätyä latentissa laskennassaan siihen, että tarvitaan ulkoinen operaatio.

Silloin nykyisessä järjestelmässä tapahtuu esimerkiksi:

\[
LLM
\rightarrow
\boxed{\text{tool call}}
\rightarrow
\text{työkalu}
\rightarrow
\boxed{\text{rakenteinen tulos}}
\rightarrow
LLM
\]

ja sitten vastaus jatkuu.

Tämä on juuri se kohta, jossa meidän aikaisempi keskustelumme on kiinnostava: nykyiset työkalut ovat vielä usein ihmisten tietokonemaailman perillisiä. Tulevaisuudessa tuo voisi olla paljon suorempi AI-native operaatiokerros.

---

## Yhteenvedettynä sinun kirjoittamasi kysymys voisi kulkea näin

```text
"mitä tälle tekstille tehdään ensin?"
                │
                ▼
          Unicode/bytes
                │
                ▼
             tokens
                │
                ▼
           embeddings
                │
                ▼
       transformer layers
                │
       ┌────────┴─────────┐
       │                  │
 syntax/language      semantics
       │                  │
       └────────┬─────────┘
                │
         latent concepts
                │
       relations / intent
                │
          reasoning
                │
       ┌────────┴────────┐
       │                 │
   answer directly    tool needed
       │                 │
       │              operation
       │                 │
       └────────┬────────┘
                ▼
         output tokens
                │
                ▼
              text
```

Ja kaikkein olennaisin vastaus kysymykseesi on:

**eri luonnollisia kieliä ei normaalisti muuteta ensin yhdeksi eksplisiittiseksi välikieleksi. Mutta transformer oppii sisäisiä korkeaulotteisia representaatioita, joissa merkitys voi muuttua huomattavasti kielestä riippumattomammaksi kuin alkuperäinen teksti.**

Se on tavallaan **itse opittu, pehmeä ja numeerinen interlingua**, mutta ilman sanakirjaa, kielioppia tai virallista formaattia.

## Käyttäjä

ensimmäinen vaihe on unicode/bytes. x86 (ehkä myös x64) arkkitehtuuri sisältää tehokkaita ascii operaatioita. Nyt teen tätä selaimessa jonka alla on lukuisia kerroksia softaa. Eikö tokeneiksi pilkkominen + muutos vektoreiksi kannattaisi tehdä suoraan raudalla?

## Avustaja

Kyllä, **teknisesti voisi**, ja osa siitä kannattaa jo tehdä lähellä rautaa. Mutta tokenisointi ja embedding ovat kaksi hyvin erilaista tapausta.

### 1. Tokenisointi voidaan hyvin siirtää "front-end-raudalle"

Nykyinen ketju on suunnilleen:

\[
\text{näppäimistö}
\rightarrow \text{selain}
\rightarrow \text{UTF-8/HTTP}
\rightarrow \text{palvelin}
\rightarrow \text{tokenizer}
\rightarrow \text{token-ID:t}
\]

Esimerkiksi:

```text
"tietokoneella"
```

muuttuu ehkä joksikin:

```text
[1837, 4912, 731]
```

Tämän voisi periaatteessa tehdä omalla piirillä:

\[
UTF8\ bytes \rightarrow TokenIDs
\]

FPGA:lla, ASICilla tai NPU:n erityisellä input-yksiköllä.

Siellä voisi olla suoraan:

- UTF-8-validointi
- Unicode-normalisointi
- token-sanasto
- byte/subword-haku
- token-ID:n muodostus
- mahdollisesti kontekstin pakkaus.

Ei käyttöjärjestelmäprosesseja, Pythonia tai edes tavallista CPU-koodia tarvittaisi.

---

## 2. Mutta embeddingiksi muuttaminen sopii rautaan vielä paljon paremmin

Kun token-ID on esimerkiksi

\[
731
\]

embedding tarkoittaa yksinkertaistettuna:

\[
E[731]
\]

missä \(E\) on valtava matriisi.

Jos embeddingin dimensio olisi kuvitteellisesti 4096:

\[
731
\rightarrow
[0.137,-0.812,0.021,\ldots]_{4096}
\]

Tämä on käytännössä **muistihakua**.

Sen vuoksi acceleratorilla voidaan tehdä suoraan:

```text
Token ID
   │
   ▼
Embedding memory
   │
   ▼
vector
```

ja vektori voidaan syöttää suoraan ensimmäiselle transformer-kerrokselle.

Ei ole mitään erityistä syytä siirtää sitä takaisin CPU:lle.

Voitaisiin siis rakentaa:

```text
UTF-8
  │
  ▼
┌────────────────┐
│ tokenizer HW   │
└───────┬────────┘
        │ token IDs
        ▼
┌────────────────┐
│ embedding RAM  │
└───────┬────────┘
        │ vectors
        ▼
┌────────────────┐
│ transformer    │
│ accelerator    │
└────────────────┘
```

Tämä on erittäin järkevä arkkitehtuuri.

---

# Mutta tokenisointi on yllättävän ikävä rautafunktio

Embedding on helppo.

Tokenisointi ei ole yhtä kaunis.

Esimerkiksi BPE-/subword-tokenisaattori joutuu käsittelemään vaihtelevan pituisia merkkijonoja ja sanastohakuja:

```text
t
ti
tie
tiet
tieto
tietokone
...
```

ja ratkaisemaan, mikä segmentointi kyseiselle mallille kuuluu.

Se on:

- epäsäännöllistä
- branchy
- paljon taulukkohakuja
- muuttuvapituisia sekvenssejä.

CPU on tällaisessa työssä varsin hyvä.

Sen sijaan GPU rakastaa:

\[
4096\times4096
\]

matriisikertolaskuja.

Siksi ei ole itsestään selvää, että tokenisaattorille kannattaa uhrata paljon piipinta-alaa.

---

## Ja vielä tärkeämpi ongelma: tokenizer ei ole universaali

Jos tekisimme prosessoriin käskyn:

```text
TOKENIZE_UTF8
```

mitä sen pitäisi tarkoittaa?

Yhdellä mallilla:

```text
"tietokoneella"
→ [17, 821, 91]
```

toisella:

```text
"tietokoneella"
→ [7218, 33]
```

kolmas malli saattaa käyttää byte-tokeneita.

Tokenizer kuuluu tällä hetkellä pitkälti **malliin**, ei luonnolliseen kieleen.

Tämä tekee kovakoodatusta ASIC-tokenizerista vähemmän houkuttelevan.

Mutta ohjelmoitava tokenizer-kiihdytin olisi aivan mahdollinen.

---

# Tästä seuraa vielä radikaalimpi ajatus: poistetaan tokenizer kokonaan

Tämä liittyy erittäin hyvin aikaisempaan keskusteluumme.

Miksi ylipäätään pitäisi olla:

\[
Unicode
\rightarrow token
\rightarrow vector
\]

Voitaisiin kouluttaa verkko ottamaan suoraan:

\[
\boxed{bytes \rightarrow neural representation}
\]

Esimerkiksi UTF-8:n tavut:

```text
74 69 65 74 6f ...
```

voisivat mennä suoraan verkkoon.

Tai Unicode-codepointit:

\[
U+0074,U+0069,\ldots
\]

Silloin erillinen BPE-tokenizer katoaa.

Arkkitehtuuri olisi:

\[
\boxed{\text{byte stream}}
\rightarrow
\boxed{\text{learned encoder}}
\rightarrow
\boxed{\text{latent representation}}
\]

Tämä olisi mielestäni konseptuaalisesti paljon puhtaampi.

Ongelma on laskennan määrä: jos yksi tavallinen token vastaa keskimäärin useampaa tavua, byte-tason sekvenssistä tulee paljon pidempi. Transformerissa pitkä konteksti maksaa.

Siksi tokenisaatio on myös **pakkausmenetelmä**:

\[
\text{monta byteä}\rightarrow\text{yksi laskentayksikkö}.
\]

---

# Voitaisiin kuitenkin tehdä opittu rautatason kompressori

Minusta tämä on kaikkein kiinnostavin mahdollisuus.

Sen sijaan että ihmiset määrittelevät BPE-sanaston:

```text
"information"
"tion"
"ing"
...
```

raudan etupää voisi olla pieni neuroverkko:

\[
bytes
\rightarrow
\boxed{\text{learned semantic compressor}}
\rightarrow
latent\ vectors
\]

Esimerkiksi:

```text
UTF-8 byte stream
      │
      ▼
local neural encoder
      │
      ▼
latent unit
latent unit
latent unit
      │
      ▼
large reasoning model
```

Silloin ei välttämättä olisi mitään ihmisen määrittelemiä "tokeneita".

Tämä vastaisi todella hyvin meidän aikaisempaa ajatustamme:

> Älä pakota AI:ta käyttämään ihmisille rakennettua välikieltä.

---

## Entä x86:n merkkijono-operaatiot?

x86:n historialliset `MOVS`, `SCAS`, `CMPS` jne. eivät nykyään ole ehkä kiinnostavin kohta. Modernissa toteutuksessa hyödyllisempiä ovat SIMD-vektorikäskyt:

\[
128/256/512\text{ bit}
\]

joilla voidaan esimerkiksi käsitellä 16, 32 tai 64 tavua rinnakkain:

```text
UTF-8 validation
byte classification
delimiter search
ASCII range detection
```

yhdellä kertaa.

Eli nykyiselläkin CPU:lla tokenizer voidaan tehdä huomattavan tehokkaaksi ilman, että merkkejä käsitellään yksitellen.

---

# Mutta kokonaiskustannuksessa selaimesi ei ole tässä suurin ongelma

Tämä on ehkä yllättävä kohta.

Kun kirjoitit tämän viestin, ehkä muutama sata tavua pitää:

\[
UTF8\rightarrow tokens
\]

Tämä on suhteellisen pieni laskenta.

Sen jälkeen LLM tekee valtavan määrän esimerkiksi:

\[
matrix\times vector
\]

ja

\[
matrix\times matrix
\]

-operaatioita jokaisella transformer-kerroksella ja jokaiselle generoimalleen tokenille.

Mittakaavat ovat aivan eri luokkaa.

Karkeasti:

```text
tokenisointi       █
transformer        ███████████████████████████████████████
```

Siksi acceleratorien suunnittelussa kannattaa ensisijaisesti käyttää transistorit:

- matriisikertolaskuihin
- nopeaan mallipainojen muistiin
- attentioniin
- KV-cacheen
- muistikaistan leveyteen.

Tokenisaation optimointi ei yleensä muuta kokonaisuutta dramaattisesti.

---

Mutta **arkkitehtonisesti olet mielestäni oikeassa**. Jos rakennettaisiin puhtaalta pöydältä nimenomaan AI-tietokone eikä nykyinen Unix/Windows-kone, johon AI on jälkikäteen lisätty, rajapinta voisi olla:

\[
\boxed{\text{human input}}
\rightarrow
\boxed{\text{neural input processor}}
\rightarrow
\boxed{\text{latent machine}}
\]

eikä:

\[
\text{browser}
\rightarrow
HTTP
\rightarrow
Unicode library
\rightarrow
tokenizer library
\rightarrow
integer array
\rightarrow
embedding kernel
\rightarrow
GPU.
\]

Ja kaikkein puhtaimmassa versiossa **token-ID saattaa itsekin olla tarpeeton historiallinen välivaihe**. Raudan input-encoder voisi muuttaa ihmisen tekstin suoraan latentiksi vektorivirraksi, jonka varsinainen reasoning-kone vastaanottaa. Se olisi jo aidosti AI-native tietokonearkkitehtuuri.

## Käyttäjä

mikä on BPE

## Avustaja

BPE = **Byte Pair Encoding**. Se on tokenisointimenetelmä: tapa pilkkoa teksti osiin, joita kielimalli käsittelee tokeneina.

Perusidea on yksinkertainen. Aloitetaan hyvin pienistä yksiköistä, esimerkiksi merkeistä tai tavuista:

```text
t i e t o k o n e
```

Sitten katsotaan koulutusaineistosta, mitkä vierekkäiset parit esiintyvät usein, ja yhdistetään niitä uusiksi yksiköiksi.

Esimerkiksi jos `t` + `i` esiintyy usein:

```text
t i  ->  ti
```

Sitten ehkä:

```text
ti e -> tie
```

ja myöhemmin:

```text
tieto + kone -> tietokone
```

Näin muodostuu sanasto, jossa on esimerkiksi:

```text
t
ti
tie
tieto
kone
tietokone
ing
tion
the
...
```

Kun uusi teksti tulee mallille, tokenizer yrittää esittää sen näiden opittujen osien avulla.

Esimerkiksi:

```text
tietokoneella
```

voisi jonkin tokenizerin sanastolla jakautua vaikka:

```text
tietokone | ella
```

tai:

```text
tieto | kone | ella
```

tai paljon pienempiin osiin. Tarkka jako riippuu mallin tokenizerista.

### Miksi tätä tehdään?

Jos jokainen kokonainen sana olisi oma token, sanaston pitäisi olla valtava:

```text
koira
koiran
koiralla
koirasta
koiralle
...
```

Suomen kaltaisessa taivutuskielessä tämä olisi erityisen hankalaa.

Jos taas jokainen kirjain olisi token:

```text
k o i r a l l a
```

sekvenssit olisivat hyvin pitkiä ja transformer joutuisi tekemään enemmän laskentaa.

BPE tekee kompromissin:

\[
\text{kirjain/tavu} < \boxed{\text{subword-token}} < \text{kokonainen sana}
\]

Yleiset sanat tai sananosat voivat olla yksi token, harvinaiset sanat hajotetaan pienemmiksi osiksi.

### Nimi "Byte Pair Encoding" on hieman historiallinen

Alkuperäinen BPE oli itse asiassa datanpakkausalgoritmi. Siinä korvattiin yleisimmin esiintyviä byte-pareja uusilla symboleilla.

NLP:ssä sama periaate otettiin käyttöön sanaston muodostamiseen.

Nykyiset tokenizerit eivät kaikki ole täsmälleen klassista BPE:tä. Käytössä on myös esimerkiksi:

- byte-level BPE
- WordPiece
- Unigram
- SentencePiece-tyyppisiä järjestelmiä.

Mutta yhteinen ajatus on sama: **opitaan järkevän kokoinen joukko tekstin palasia, jotta koko tekstiä ei tarvitse käsitellä kirjain kerrallaan eikä jokaista mahdollista sanaa tarvitse tuntea erikseen.**

Tässä meidän aiemmassa keskustelussamme BPE:n kiinnostava puoli on juuri se, että se on vielä varsin **ihmisen suunnittelema diskreetti välikerros**:

\[
\text{bytes}
\rightarrow
\boxed{\text{BPE-tokenit}}
\rightarrow
\text{vektorit}
\]

ja voisi kysyä, miksei tulevaisuuden AI-rauta tekisi suoraan:

\[
\text{bytes}
\rightarrow
\boxed{\text{opittu latentti kompressio}}
\rightarrow
\text{vektorit}
\]

ilman mitään eksplisiittistä BPE-sanastoa.

## Käyttäjä

Onko käytännön kokemusta miten eri kielet soveltuvat bytes->vektorit muunnokseen, kiinan merkit, tavukielet, hyvin suppeat sanavarastot, eroaako arabia käsittelyssä intiaanikielestä jossa on levy- ja tankomaisuutta kuvaavia rakenteita.

## Avustaja

On käytännön tutkimusta, ja siinä näkyy selvä ero **kirjoitusjärjestelmän**, **morfologian** ja **tokenisointitavan** välillä. Erottelen nämä, koska esimerkiksi kiinan "merkkiongelma" ja arabian "morfologiaongelma" ovat eri ilmiöitä.

## Avustaja

Kyllä, tästä on jo käytännön kokemusta, ja tulos on kiinnostava: **byte→vektori poistaa yhden kielten välisen epäreiluuden — kiinteän tokenisanaston — mutta ei tee kielistä laskennallisesti samanlaisia.** Erot vain siirtyvät seuraavalle tasolle.

ByT5 ja CANINE ovat konkreettisia esimerkkejä tokenittomista malleista. ByT5 käyttää raakoja bytejä ja CANINE merkkejä; molemmat osoittivat, että monikielinen malli voidaan tehdä ilman perinteistä subword-tokenizeria ja että ratkaisu voi olla kilpailukykyinen, erityisesti kirjoitusasun, ääntämyksen, kohinan ja harvinaisten muotojen käsittelyssä. 

Mutta kielten välillä syntyy ainakin neljä aivan eri ongelmaa.

### 1. UTF-8 itsessään suosii joitakin kirjoitusjärjestelmiä

Jos syötetään **bytejä**, englanti on sattumalta erittäin edullinen:

```text
a    = 1 byte
b    = 1 byte
...
```

Arabialainen kirjain on tyypillisesti 2 UTF-8-byteä ja tavallinen kiinalainen Han-merkki yleensä 3 byteä.

Siis karkeasti:

\[
\text{cat} \approx 3\ bytes
\]

mutta kolme kiinalaista merkkiä:

\[
漢字語 \approx 9\ bytes.
\]

Tämä ei tarkoita, että kiinankielisessä ilmauksessa olisi kolminkertainen määrä informaatiota tai että se olisi kolme kertaa vaikeampi ymmärtää. Se on Unicode/UTF-8:n historiallinen ominaisuus.

Vuoden 2024 tutkimuksessa tätä kutsutaan **byte premiumiksi**: saman sisältöinen teksti vie eri kielillä hyvin erilaisen määrän UTF-8-bytejä. Tutkimus mittasi ilmiötä yli tuhannessa kielessä. Vuoden 2026 ACL-työssä mennään jo pidemmälle ja todetaan suoraan, ettei UTF-8:aa ole suunniteltu neuroverkon tehokkuutta tai kielten välistä tasapuolisuutta varten. 

Tämä liittyy suoraan aiempaan ajatukseemme: **jos suunnittelisimme AI-tietokoneen tyhjästä, UTF-8 ei välttämättä olisi paras neuroverkon sisääntulo-ABI.**

---

## 2. Kiina on bytes-tasolla kallis mutta kielellisesti tiheä

Kiina on hauska vastakohta englannille.

Yksi merkki:

> 人

on UTF-8:ssa kolme byteä, mutta se voi kantaa suunnilleen morfeemin verran informaatiota.

Englannin:

> person

on kuusi ASCII-byteä.

Siksi bytejen lukumäärä ei kerro semanttisen informaation määrää.

Lisäksi kiinassa on suhteellisen vähän englannin tai suomen kaltaista taivutusmorfologiaa. Mallin ei tarvitse oppia:

```text
koira
koiran
koiralla
koiraltani
koirillammekaan
```

samassa määrin kuin suomessa.

Toisaalta kiinalla on sanasegmentointiongelma:

```text
北京大学
```

voidaan joutua ymmärtämään rakenteena

\[
北京 + 大学
\]

eikä Unicode itsessään kerro sananrajoja.

Byte-malli voi kuitenkin itse oppia:

\[
3\ bytes \rightarrow yksi\ Han\ character
\]

ja sitten:

\[
characters \rightarrow words/concepts.
\]

UTF-8:n rakenne on deterministinen, joten 3-bytisen merkin rajojen oppiminen ei sinänsä ole vaikea ongelma.

Kiinalaisista malleista on lisäksi saatu näyttöä siitä, että tokenisointiratkaisu todella voi muuttaa sitä, miten merkkien merkitys representoituu; tokenirajat eivät aina osu kielen luonnollisiin rakenteisiin. 

---

## 3. Tavukirjoitus olisi itse asiassa varsin mukava opitulle byte-encoderille

Jos "tavukielellä" tarkoitat **tavukirjoitusta**, kuten esimerkiksi japanin kanaa tai Cherokee-tavustoa, tilanne voisi olla varsin luonteva.

Kirjoitusjärjestelmä on jo tehnyt:

\[
\text{äännejono} \rightarrow \text{tavu}
\]

Esimerkiksi japanin:

> か

edustaa suunnilleen `/ka/`.

Byte-encoder näkee ensin vain:

\[
E3\ 81\ 8B
\]

mutta koska tuo kolmen byten sarja esiintyy aina yhdessä, pieni paikallinen neuroverkko voi erittäin helposti oppia:

\[
[E3,81,8B]\rightarrow V_{\text{ka}}
\]

Silloin byte-tason rumuus häviää jo aivan ensimmäisessä encoderissa.

Tämä on hyvä peruste sille, että tulevaisuuden arkkitehtuurin ei ehkä pitäisi antaa varsinaiselle reasoning-transformerille jokaista byteä erikseen.

Parempi:

\[
bytes
\rightarrow
\boxed{\text{local learned encoder}}
\rightarrow
\boxed{\text{muuttuvapituiset latentit yksiköt}}
\rightarrow
reasoning
\]

Juuri tämän suuntaiseen ratkaisuun esimerkiksi Byte Latent Transformer (BLT) menee: se ryhmittelee bytejä dynaamisiksi *patch*-yksiköiksi sen sijaan, että päätransformer käyttäisi saman verran laskentaa jokaiselle bytelle. BLT:n julkaistut kokeet näyttivät, että byte-pohjainen malli voidaan saada skaalassa tokenipohjaisen mallin tasolle ja joissakin ominaisuuksissa sen yli. 

---

# Arabia on paljon kiinnostavampi tapaus

Arabian vaikeus ei ole ensisijaisesti arabialainen kirjaimisto.

Sen byte-esitys voidaan oppia.

Ongelma on **kielen rakenne**.

Arabian morfologiassa esimerkiksi juuri

\[
k-t-b
\]

liittyy kirjoittamiseen, mutta siitä muodostuu erilaisia rakenteita lisäämällä ja muuttamalla ainesta **juuren sisällä**, ei pelkästään liimaamalla päätteitä loppuun.

Hyvin yksinkertaistaen:

\[
KTB + pattern \rightarrow word
\]

Tämä on eri ongelma kuin suomen:

\[
talo + ssa + ni
\]

joka on suhteellisen lineaarisesti segmentoitavissa.

BPE rakastaa jälkimmäistä, jos aineistoa on tarpeeksi:

```text
talo | ssa | ni
```

Arabian juuri–malli-rakenne voi olla vaikeampi saada näkyviin yhtenäisinä substring-tokenina.

Byte-/character-mallilla on tässä potentiaalinen etu: se **ei ole pakotettu uskomaan**, että kielen merkitysyksiköt ovat jatkuvia substringejä.

Syvempi verkko voi oppia yhteyden:

\[
k\quad ...\quad t\quad ...\quad b
\]

vaikka niiden välissä on muuta ainesta.

Mutta se vaatii enemmän itse mallilta.

Ja käytännön kokeissa ei ole löytynyt sääntöä "byte-malli voittaa aina morfologisesti vaikealla kielellä". Vuoden 2025 vertailussa ByT5 ja subword-pohjainen mT5 vaihtelivat huomattavasti kielen ja morfologisen tehtävän mukaan: esimerkiksi joissakin Hindi-tehtävissä ByT5 oli selvästi parempi, kun taas joissakin arabian ja turkin tehtävissä mT5 oli parempi. 

---

# Sitten tullaan mainitsemaasi intiaanikieleen

Kuvaamasi **levymäistä, tankomaista, joustavaa jne. objektia ilmaiseva rakenne** kuulostaa erittäin paljon navajon ja muiden athabaskikielten *classificatory verb* -järjestelmältä.

Navajossa esimerkiksi käsiteltävä objekti voi vaikuttaa käytettävään verbivartaloon sen mukaan, onko se:

- hoikka ja jäykkä
- hoikka ja joustava
- litteä ja joustava
- pyöreähkö kiinteä esine
- irtonainen massa
- avoin astia jne. 

Tämä on AI:n kannalta todella kiinnostava esimerkki.

Englanniksi voidaan sanoa:

> give me X

ja `give` pysyy pitkälti samana.

Navajossa "käsittele/anna/siirrä X" -verbin muoto voi jo itsessään koodata jotain X:n **geometriasta ja fysikaalisesta luonteesta**.

Siis kielen representaatiossa saattaa olla jotakin:

\[
VERB =
Action
+
ShapeClass
+
Consistency
+
Aspect
+\ldots
\]

Navajon verbit ovat lisäksi morfologisesti erittäin monimutkaisia ja voivat sisältää suuren määrän morfeemeja; kieltä kuvataan usein polysynteettiseksi tai ainakin hyvin polysynteettisiä piirteitä sisältäväksi. 

---

## Tässä byte-malli voi olla periaatteessa erittäin hyvä

Ajatellaan harvinaista navajon verbimuotoa, jota BPE ei ole juuri koskaan nähnyt.

BPE saattaa tehdä:

```text
foo | bar | xyz | ...
```

puhtaasti korpusfrekvenssin perusteella, eikä jako välttämättä vastaa morfologiaa.

Byte-malli ei tee tätä oletusta.

Se voi vähitellen oppia:

\[
\text{prefix sequence}
+
\text{verb stem}
+
\text{phonological changes}
\rightarrow
\text{latent structure}
\]

ja esimerkiksi latentisti saada jotain tämän kaltaista:

\[
[
\text{TRANSFER},
\text{SLENDER-RIGID},
\text{OBJECT},
\text{PERFECTIVE},
...
]
\]

Huomaa olennainen ero:

**tuo latentti representaatio voi olla semanttisesti paljon siistimpi kuin kirjoitettu sana itse.**

Tästä päästään suoraan siihen alkuperäiseen ajatukseesi yhteisestä väli-representaatiosta.

Englanti:

> carry the rod

Navajo:

> yksi kompleksinen verbimuoto

voivat syvällä verkossa lähestyä jotain:

\[
\boxed{
ACTION=carry,\quad
OBJECT.shape=long/rigid
}
\]

vaikka pintakielet rakentavat asian aivan eri tavalla.

---

# Tässä mielessä "hyvin suppea sanavarasto" ei välttämättä ole yksinkertainen kieli

Tämä on tärkeä ero.

Jos kielessä on vähän vapaita sanavartaloita mutta erittäin tuottelias morfologia, se voi muodostaa valtavan määrän sanamuotoja:

\[
R \times A \times B \times C \times D \rightarrow
\text{miljoonia mahdollisia forms}
\]

Vaikka juuria olisi suhteellisen vähän.

BPE:n näkökulmasta tämä voi olla hankalaa, koska kokonaisia muotoja esiintyy harvoin.

Byte-/character-mallille rakenne voi olla luonnollisempi:

\[
\text{pienet toistuvat muodot}
\rightarrow
\text{opitut abstraktiot}.
\]

Siksi vuoden 2026 tutkimuksissa näkyy edelleen vahva yhteys morfologisen monimutkaisuuden, huonon tokenisaation ja heikomman mallisuorituksen välillä erityisesti vähäresurssisissa kielissä. 

---

## Mutta kaikkein suurin tekijä ei lopulta ole kielioppi vaan aineiston määrä

Tämä on helppo unohtaa.

Kuvitellaan kaksi kieltä:

**A:** erittäin monimutkainen polysynteettinen kieli, 10 biljoonaa hyvää harjoitteluesimerkkiä.

**B:** yksinkertainen analyyttinen kieli, 100 000 lausetta.

Todennäköisesti A:sta syntyy paljon parempi representaatio.

Byte-mallin hienous on siinä, että **B:n kirjoitusjärjestelmälle ei tarvitse ensin rakentaa tokenizeria tai sanastoa**. Se pystyy ainakin vastaanottamaan minkä tahansa UTF-8-kielen:

\[
256\ byte\ values
\]

riittävät teoriassa kaikkien Unicode-kielten syöttämiseen.

ByT5:n kokeissa tämä universaalisuus oli todellinen etu, ja esimerkiksi lähes sadan kielen grapheme→phoneme-kokeissa byte-pohjainen ByT5 pärjäsi tokenipohjaista mT5:tä paremmin. 

---

### Minusta tästä seuraa arkkitehtuurisesti kiinnostava johtopäätös

Jos rakennettaisiin meidän hahmottelemaamme AI-native konetta, en ehkä käyttäisi edes puhtaasti:

\[
UTF8\ bytes \rightarrow transformer
\]

vaan:

\[
\boxed{\text{raw symbolic stream}}
\]

\[
\downarrow
\]

\[
\boxed{\text{pieni opittu paikallinen encoder}}
\]

\[
\downarrow
\]

\[
\boxed{\text{adaptiiviset latentit yksiköt}}
\]

\[
\downarrow
\]

\[
\boxed{\text{kielestä riippumattomampi semanttinen reasoning}}
\]

Paikallinen encoder voisi itse oppia, että:

- englannissa 5 byteä muodostaa hyödyllisen morfeemin,
- kiinassa 3 byteä muodostaa yhden merkin mutta kaksi merkkiä yhden käsitteen,
- japanissa 3 byteä muodostaa yhden kana-tavun,
- arabiassa kannattaa huomioida ei-jatkuvia juuri–malli-suhteita,
- navajossa pitkä verbimuoto kannattaa hajottaa aivan erilaiseksi latentiksi rakenteeksi.

**Sen ei siis tarvitsisi päättää etukäteen, mikä on "sana", "merkki", "tavu" tai "morfeemi".** Ne voisivat olla koulutuksessa syntyviä välirepresentaatioita.

Tämä on minusta yksi vahvimmista argumenteista tokenittoman AI-arkkitehtuurin puolesta: ihmiskielten typologia on niin erilainen, että yksi ihmisen etukäteen määräämä BPE-tyyppinen "tekstin perusyksikkö" on väistämättä kompromissi.

## Käyttäjä

palataan vielä työkaluihin. Nyt hyvin tuettu python on siinä mielessä hyvä että sillä voi kaikissa käyttöjärjestelmissä tehdä lähes kaikki asiat joita AI:n pitää tehdä joko diskreettiin laskentaan, kuvankäsittelyyn, tekstitiedostojen ja eri formaattien muuntamiseen. Mutta mikä tälle toolchainille olisi paras natiivi AI toolchain. AI:lla on hyvä olla mahdollisuus ohjelmointiin ongelmanratkaisussa, mutta mikä on luontevin AI sopiva ympäristö, kieli tai  ongelmanratkaisutapa joka nyt ratkaistaan pythonilla?

## Avustaja

Tähän kannattaa erottaa **“AI:n ohjelmointikieli”** ja **“AI:n suoritusympäristö”**. Paras ratkaisu ei todennäköisesti ole yksi uusi Python-korvike, vaan pieni AI-native IR + turvallinen runtime + vakioidut kyvykkyydet; vertaan sitä muutamaan nykyiseen teknologiaan, jotka ovat jo lähellä tätä.

## Avustaja

Minusta luontevin korvaaja Pythonille **ei olisi uusi yleiskäyttöinen ohjelmointikieli**, vaan yhdistelmä:

\[
\boxed{\text{tyypitetty laskentagraafi}}
+
\boxed{\text{vakioidut capabilityt}}
+
\boxed{\text{deterministinen runtime}}
\]

Python jäisi lähinnä yhteensopivuuskerrokseksi nykyiseen ohjelmistomaailmaan.

Nykyisistä tekniikoista lähimmäksi eri osia tulevat **MLIR**, **WebAssembly Component Model/WASI** ja joillakin data-alueilla **Apache Arrow**. Ne eivät yksinään ratkaise asiaa, mutta niistä saisi erittäin hyvän pohjan. MLIR on nimenomaan monitasoinen IR, joka pystyy esittämään sekä korkean tason dataflow-graafeja että laskemaan niitä kohti laitteistokohtaista koodia. 

## Pythonin todellinen tehtävä AI:lle

Kun minä teen Pythonilla:

```python
import pandas as pd

df = pd.read_csv("data.csv")
x = df[df.current > 10]
print(x.groupby("charger").energy.sum())
```

Python ei ole tässä oikeastaan ongelmanratkaisun ydin.

Semanttinen ohjelma on:

\[
\operatorname{read\_table}
\rightarrow
\operatorname{filter}
\rightarrow
\operatorname{group}
\rightarrow
\operatorname{sum}.
\]

Python on vain tapa **kirjoittaa tämä laskentagraafi tekstinä**.

Sitten tapahtuu suuri määrä välityötä:

```text
LLM
 ↓
Python-merkkijono
 ↓
parser
 ↓
Python AST
 ↓
Python bytecode
 ↓
interpreter
 ↓
pandas
 ↓
NumPy / Arrow / C
 ↓
CPU
```

AI-native järjestelmä voisi aloittaa suoraan kohdasta:

```text
Table #71
   │
FILTER current > 10
   │
GROUP charger
   │
SUM energy
   ▼
Table #83
```

Sitä ei tarvitsisi koskaan kirjoittaa Pythoniksi.

---

# Millainen AI:n "ohjelmointikieli" sitten olisi?

Valitsisin perustaksi **tyypitetyn funktionaalisen dataflow/SSA-graafin**.

Ei ensisijaisesti:

```text
x = ...
for i in ...
if ...
```

vaan:

\[
v_3 = Filter(v_1,\ current>10)
\]

\[
v_4 = Group(v_3,\ charger)
\]

\[
v_5 = Sum(v_4,\ energy)
\]

Jokainen `v` olisi viittaus olioon eikä muuttuva muistipaikka.

Graafi voisi olla esimerkiksi:

```text
             ┌── constant(10 A)
             │
Table ── Filter ── Group(charger) ── Sum(energy)
```

Tämä ratkaisee monta asiaa kerralla.

Runtime näkee **koko laskennan ennen suorittamista**. Siksi se voi:

- yhdistää operaatioita
- rinnakkaistaa niitä
- käyttää SIMD:iä
- siirtää laskennan GPU:lle
- käyttää tietokantamoottoria
- streamata dataa
- olla luomatta turhia välituloksia.

Tämä on juuri SQL:n suuri idea. Kun kirjoitat:

```sql
SELECT charger, SUM(energy)
...
GROUP BY charger
```

et kerro tietokannalle, missä järjestyksessä CPU:n pitää lukea muistia.

Sanot:

\[
\boxed{\text{mitä haluan}}
\]

ja query optimizer ratkaisee:

\[
\boxed{\text{miten se tehdään}}.
\]

**AI-native ohjelmoinnin pitäisi mielestäni olla paljon enemmän SQL:n kuin Pythonin kaltaista.**

---

# Mutta pelkkä dataflow ei riitä

AI tarvitsee hyvin erilaisia laskennan "dialekteja".

Kuvittelisin ytimen ympärille esimerkiksi:

```text
scalar
    exact integer
    rational
    float
    decimal
    complex

array
    map
    reduce
    convolution
    fft
    matrix solve

table
    filter
    join
    group
    aggregate

text
    search
    tokenize
    parse
    regex
    transform

image
    decode
    crop
    resize
    segment
    composite

graph
    traverse
    query
    shortest_path

logic
    predicate
    constraint
    solve
    prove

stream
    map
    filter
    window
    aggregate
```

Tämä muistuttaa erittäin paljon MLIR:n **dialect**-ajatusta: samassa IR-kehyksessä voidaan säilyttää korkean tason operaatiot ja laskea niitä vähitellen alemman tason representaatioiksi. 

Esimerkiksi:

\[
Image.Resize
\]

voidaan myöhemmin laskea:

\[
TensorOps
\rightarrow
VectorOps
\rightarrow
GPU/CPU
\]

ilman että AI kirjoittaa mitään näistä vaiheista.

---

## Ja sitten tarvitaan toinen asia: maailman kanssa toimiminen

Tässä WebAssembly Component Model on minusta tällä hetkellä erittäin kiinnostava esikuva.

Siinä komponentti määrittelee eksplisiittisesti **mitä rajapintoja se tarvitsee ja mitä se tarjoaa**. WIT-rajapinnat ovat tyypitettyjä, ja komponentti voi olla kokonaan vailla esimerkiksi filesystem- tai network-oikeutta, ellei kyseistä capabilitya ole sille annettu. 

AI:lle voisi antaa:

```text
capability http
capability database
capability image
capability spreadsheet
capability compiler
capability device.evcharger
```

Sen sijaan että sille annetaan:

```text
bash
```

ja sanotaan:

> yritä pärjätä.

Esimerkiksi:

```text
http.get(URL)
```

olisi todellinen järjestelmäoperaatio.

Ei:

```bash
curl -L --connect-timeout 20 ...
```

WASI:n Component Model tekee jo osan tästä ajatuksesta: WIT määrittelee tyypitettyjä interfaceja, ja nykyisessä WASI:ssa on esimerkiksi korkean tason HTTP-rajapinta komentorivisemmän I/O:n lisäksi. Kesäkuussa 2026 julkaistu WASI 0.3 toi rajapintoihin myös natiivin asyncin, streamit ja futuret. 

---

# Tekisin kuitenkin yhden ratkaisevan eron WASIin

WASI on edelleen tarkoitettu myös **ihmisten nykyisten ohjelmien ajamiseen**.

AI-native ABI:n ei tarvitse säilyttää esimerkiksi:

```text
stdin
stdout
stderr
argv
cwd
environment variables
```

pääabstraktioina.

AI voisi saada:

```text
HTTPResponse
Image
Table
Document
DatabaseQueryResult
SourceTree
ProcessResult
```

suoraan tyypitettyinä objekteina.

Esimerkiksi nykyinen:

```text
curl
 ↓
stdout bytes
 ↓
JSON parser
 ↓
dict
```

muuttuisi:

\[
HTTP.Get(url)
\rightarrow
Response<Json>.
\]

---

# Dataa ei myöskään pitäisi serialisoida koko ajan

Python-työkaluketjussa syntyy helposti:

\[
DataFrame
\rightarrow CSV
\rightarrow bytes
\rightarrow parser
\rightarrow DataFrame.
\]

AI-native ympäristössä pitäisi olla yhteinen muistirepresentaatio.

Taulukkodatalle nykyinen Apache Arrow on erittäin hyvä esimerkki: se määrittelee kieliriippumattoman muistiformaatin, joka on suunniteltu SIMD-ystävälliseksi ja mahdollistaa monissa tapauksissa datan jakamisen ilman kopiointia. 

Siis:

```text
AI operation
      │
      ▼
   TableRef
      │
 ┌────┼──────────┐
 ▼    ▼          ▼
SQL  statistics  plotting
```

kaikki voisivat käsitellä samaa dataobjektia.

Ei:

```text
write CSV
read CSV
write temp.npy
read temp.npy
```

---

# Entä varsinainen "uuden ohjelman keksiminen"?

Tätä ei saa hukata.

Pelkkä BusyBox-tyyppinen funktiokokoelma ei riitä, koska Pythonin valtava vahvuus AI:lle on:

> jos valmista työkalua ei ole, voin **kirjoittaa uuden algoritmin**.

AI-native IR:n täytyy siis olla **Turing-täydellinen tai ainakin päästä yleislaskentaan tarvittaessa**.

Mutta sen ei tarvitse olla Pythonin kaltainen.

Esimerkiksi AI voisi muodostaa suoraan:

```text
func gcd(a:i64, b:i64) -> i64 {
    loop(a,b) {
        branch b == 0 {
            true  -> return a
            false -> recur(b, a % b)
        }
    }
}
```

Tämäkin teksti olisi vain debug-näkymä.

Oikeasti AI tuottaisi AST/SSA-graafin:

```text
        ┌───────────┐
        │   b == 0  │
        └─────┬─────┘
          yes │ no
              │
       return a    ── mod(a,b)
                       │
                    loop(b,r)
```

Ei parseria lainkaan.

---

## Tässä AI:lla olisi itse asiassa etu ihmiseen verrattuna

Ihmisen ohjelmointikielessä tarvitaan:

```text
{
}
;
indentation
variable names
imports
module names
comments
```

koska ihmisen pitää lukea ohjelmaa.

AI:n graafissa voidaan käyttää:

```text
node 938271
node 938272
edge 938271→938272
```

eikä nimiä edes tarvita.

Esimerkiksi ihmisen:

```python
average_charging_power
```

on AI:n sisäisessä representaatiossa turhaa informaatiota.

Tyypit ja datavirta kertovat merkityksen.

Ihmiselle voidaan tarvittaessa generoida jälkikäteen:

```python
average_charging_power = energy / duration
```

---

# Millä nykytekniikalla rakentaisin tämän nyt?

Jos tätä pitäisi oikeasti prototypoida vuonna 2026, tekisin suunnilleen:

```text
                 LLM
                  │
          typed operation graph
                  │
         ┌────────┴─────────┐
         │                  │
    semantic ops       custom code
         │                  │
         └───────┬──────────┘
                 ▼
              MLIR
                 │
       optimization/lowering
          ┌──────┼──────┐
          ▼      ▼      ▼
         CPU    GPU    WASM
                        │
                        ▼
                Component runtime
                        │
              typed capabilities
```

**MLIR** olisi compiler-puolen selkäranka. Se on jo suunniteltu juuri korkean tason dataflow'n ja laitteistokohtaisen koodin väliseksi monitasoiseksi representaatioksi. Lisäksi sillä on tekstimuoto, ohjelmallisesti käsiteltävä muistimuoto ja kompakti binäärimuoto. 

**WebAssembly Component Model + WIT** olisi hyvä esikuva — ja mahdollisesti myös toteutus — turvalliselle portable runtime- ja capability-rajalle. Komponentit kommunikoivat tyypitettyjen interfacejen kautta, ja WIT erottaa rajapinnan toteutuskielestä. 

**Arrow-tyylinen yhteinen muistimalli** sopisi rakenteiselle datalle, jotta komponentit eivät jatkuvasti serialisoisi tietoa toisilleen. 

---

## En siis valitsisi AI:n kieleksi Pythonia, Rustia, C:tä enkä Lisp:iä

Jos minun pitäisi nimetä yksi abstraktio, sanoisin:

> **AI-native programming environment = typed functional dataflow IR with capability-based effects.**

Suomeksi vähän rumasti:

**tyypitetty funktionaalinen laskentagraafi, jonka sivuvaikutukset tapahtuvat eksplisiittisten kyvykkyyksien kautta.**

Siinä olisi kaksi erilaista asiaa:

\[
\boxed{\text{Compute}}
\]

puhdas, deterministinen laskenta:

```text
filter
map
solve
fft
parse
render
```

ja

\[
\boxed{\text{Effect}}
\]

maailmaan vaikuttaminen:

```text
read sensor
send HTTP
change EV current
store object
send message
```

Tämä jako olisi AI-agentille erittäin arvokas. Runtime voisi tietää automaattisesti, että:

```text
MatrixMultiply
```

ei voi tehdä mitään vaarallista, mutta:

```text
Bank.Transfer
```

muuttaa maailmaa.

---

### Python säilyisi silti pitkään

Ei siksi, että sen arkkitehtuuri olisi AI:lle optimaalinen, vaan koska sen kirjastomaailma on valtava:

\[
\text{NumPy + SciPy + OpenCV + pandas + PIL + SymPy + ...}
\]

AI-native ympäristö voisi aluksi kapseloida nämä capabilityiksi:

```text
image.segment(...)
statistics.fit(...)
symbolic.integrate(...)
```

ja toteutus olisi taustalla Pythonia.

Sitten toteutuksia voisi yksi kerrallaan siirtää native-runtimelle ilman, että AI:n ohjelma muuttuu.

Tämä on ehkä tärkein arkkitehtuurinen ominaisuus: **AI ei saisi tietää eikä välittää, toteuttaako `image.resize` tänään Python/Pillow, huomenna Rust ja ensi vuonna suoraan NPU-rauta.**

Silloin päästäisiin todella siitä tilanteesta, jota alun perin kritisoit: AI ei enää "rämpyttäisi tietokonetta kuten ihminen", vaan **kuvaisi haluamansa laskennan**, ja järjestelmä päättäisi miten se suoritetaan.

## Käyttäjä

tietokoneessa kuitenkin IP osoittaa ohjelmaan ja suorittaa käskyjä joita matalimmalla tasolla kuvataan assembler ohjelmoinissa. Pelkillä opkoodeilla on tuotettu kaikki python vaikka välissä olisi kymmenen virtuaalikonetta. Tietokone ei voi suorittaa A -> B, vaan se voi suorittaa mov A, B. mitä tarkoitat graafeilla? Mikä on ohjelmointi jossa käytetään "graafeja". Eikö AI voisi osata suoraan yksikertaisesti x64 käskykannan ja tehdä suoraan tällä funktioita ja olioita jotka tallennetaan Objekteiksi joita AI osaa käyttää mutta ihmisen on turha yrittää debugata tai "kääntää". Eikö yksikertainen assembler ohjelma ole token jonka voi jo sinänsä "käsittää".

## Avustaja

Kyllä. Tässä kohtaa olet aivan oikeassa siinä, että **graafi ei ole mikään vaihtoehtoinen fyysinen suoritusmekanismi**. Prosessori suorittaa edelleen käskyjä järjestyksessä: `mov`, `add`, `cmp`, `jmp`, SIMD-käskyt jne.

Kun puhuin "laskentagraafista", tarkoitin **ohjelman representaatiota ennen konekoodia**.

Esimerkiksi tavallinen ohjelma:

```asm
mov rax, [A]
mov rbx, [B]
add rax, rbx
imul rax, [C]
```

voidaan ajatella datavirran kannalta näin:

```text
A ──┐
    ADD ────┐
B ──┘       MUL ──> result
            │
C ──────────┘
```

Graafin solmut ovat operaatiot `ADD`, `MUL`; reunat kertovat, minkä operaation tulos menee minkä seuraavan operaation syötteeksi.

Se ei siis suorita mitään itsestään. Lopulta siitä tuotetaan konekäskyt.

## Tämä on jo aivan normaalia kääntäjätekniikkaa

LLVM IR käyttää SSA-muotoa, jossa jokainen laskettu arvo syntyy kerran ja sen käyttökohteet ovat eksplisiittisiä. LLVM itse kuvaa IR:ää SSA-pohjaiseksi välikieleksi, ja control-flow graph muodostuu basic blockeista ja niiden välisistä haaroista. 

Esimerkiksi:

```c
c = (a + b) * d;
```

voi olla suunnilleen:

```text
%1 = add %a, %b
%2 = mul %1, %d
return %2
```

Tuo on tekstinä lineaarinen, mutta semanttisesti:

```text
a ───┐
     add ─── %1 ───┐
b ───┘              mul ─── %2
d ──────────────────┘
```

LLVM:n konekoodigeneraattorissa on ollut myös SelectionDAG-tyyppisiä rakenteita, joissa solmut ovat operaatiota ja niiden operandit muodostavat nimenomaan datavirran reunat. 

Eli "graafiohjelmointi" ei tässä tarkoita välttämättä sitä, että ihminen piirtäisi laatikoita ruudulle. Se tarkoittaa, että **ohjelman sisäinen representaatio on solmuista ja riippuvuuksista koostuva rakenne**.

---

# Mutta varsinainen kysymyksesi x64:stä on kiinnostavampi

> Eikö AI voisi vain oppia x64-käskykannan ja tehdä suoraan konekoodia?

**Kyllä voisi.**

Periaatteessa täysin mahdollinen arkkitehtuuri olisi:

\[
\text{LLM reasoning}
\rightarrow
\boxed{\text{x86-64 machine code}}
\rightarrow
CPU
\]

AI voisi generoida vaikka suoraan tavut:

```text
48 8B 07
48 03 06
48 0F AF 02
C3
```

ilman:

- Pythonia
- C:tä
- assembler-lähdekoodia
- LLVM:ää
- linker-komentoja.

Jos AI tuntee x86-64 ISA:n, ABI:n ja muistimallin, se voisi muodostaa suoraan executable code blockin.

Ja JIT-järjestelmät tekevät käytännössä juuri tätä: generoivat suoraan konekoodia muistiin ja hyppäävät siihen.

---

## Vielä pidemmälle: ei välttämättä edes assemblertekstiä

Assembler:

```asm
mov rax, rdi
add rax, rsi
ret
```

on ihmiselle tarkoitettu tekstiesitys.

CPU tarvitsee vain:

```text
48 89 F8
48 01 F0
C3
```

AI voisi siis muodostaa suoraan:

\[
[0x48,0x89,0xF8,0x48,0x01,0xF0,0xC3]
\]

ja tallentaa sen objektina:

```text
ExecutableObject #9137
architecture = x86-64
entry = offset 0
signature = (u64,u64)->u64
code = ...
```

AI tietäisi:

> Object #9137 on funktio, joka laskee kahden unsigned 64-bit luvun summan.

Ihmisen ei välttämättä tarvitsisi nähdä sitä koskaan.

Tämä sopii hyvin siihen aiempaan Object–State–Operation-malliimme.

---

# Miksi sitten ehdotin IR/graafia enkä suoraan x64:ää?

Koska **x64 on hyvä suorituskieli mutta huono ongelmanratkaisukieli**.

Esimerkiksi AI haluaa:

> laske 10 miljoonan liukuluvun summa.

Semanttisesti tämä on vain:

\[
SUM(A)
\]

Mutta x64-koodissa AI joutuu ratkaisemaan itse:

- rekisterien allokoinnin
- AVX2 vai AVX-512?
- alignment
- loop unrolling
- cache-prefetch
- tail handling
- calling convention
- stack alignment
- CPU feature detection
- exception/state preservation.

Eli AI joutuu käyttämään huomattavan osan "ajattelustaan" asioihin, jotka kone voi ratkaista deterministisesti.

Sama ongelma kuin Pythonissa, mutta toisessa ääripäässä.

Python on liian korkea ja raskas:

\[
SUM(A)
\rightarrow
Python/pandas/NumPy/\ldots
\]

x64 on liian matala:

\[
SUM(A)
\rightarrow
\text{satoja konekäskyjä ja mikroarkkitehtuurivalintoja}.
\]

Luonteva väli on:

\[
\boxed{SUM(A)}
\]

ja runtime tekee:

\[
SUM(A)
\rightarrow
\boxed{\text{x64 optimized implementation}}.
\]

---

# Mutta AI voisi aivan hyvin käyttää x64:ää "escape hatchina"

Tässä olen enemmän samaa mieltä kanssasi kuin ehkä edellisestä vastauksestani välittyi.

AI-runtime voisi antaa kaksi tasoa:

### Korkean tason primitiivit

```text
SUM
FILTER
FFT
JPEG_DECODE
SQL_QUERY
HTTP_GET
```

ja tarvittaessa:

### Raw machine code

```text
X64_CODE {
    ...
}
```

Jos AI keksii uuden algoritmin, jota runtime ei tunne:

\[
\text{AI}
\rightarrow
\boxed{\text{x64 code}}
\rightarrow
\text{execute}
\]

Se olisi tavallaan AI:n assembler-työpöytä.

---

## Ja assemblerkäsky todella voi olla AI:lle hyvin luonteva "token"

Tämä on hyvä huomiosi.

Esimerkiksi:

```asm
xor eax,eax
```

on käsitteellisesti hyvin kompakti kokonaisuus:

\[
EAX := 0
\]

AI:n ei tarvitse käsitellä sitä ihmisen tapaan kolmesta sanasta koostuvana lauseena.

Sisäinen representaatio voisi olla:

```text
Instruction {
    opcode = XOR
    dst = EAX
    src = EAX
}
```

tai jopa yksi diskreetti operaatiosymboli + operandit.

Vastaavasti:

```asm
vaddps zmm0,zmm1,zmm2
```

on semanttisesti:

\[
ZMM_0 = ZMM_1 + ZMM_2
\]

16 rinnakkaiselle `float32`:lle.

Tällainen ISA on itse asiassa huomattavasti "puhtaampi kieli" koneelle kuin Python.

---

# Mutta x64:ssä on edelleen valtavasti historiallista painolastia

Tämä on se syy, miksi en valitsisi juuri x64:ää AI:n ideaaliksi sisäiseksi ISA:ksi.

x86-64 sisältää vuosikymmenten yhteensopivuushistoriaa:

- variable-length instructions
- lukuisia addressing modeja
- legacy register -käyttäytymistä
- condition flags
- x87
- SSE/AVX-historiaa
- monimutkaisia instruction encodingeja
- ABI-konventioita, jotka eivät ole CPU:n luonnonlakeja.

AI:lle suunniteltu ISA voisi olla paljon yksinkertaisempi.

Esimerkiksi:

```text
LOAD64    r1, object17[offset]
ADD64     r3, r1, r2
VADD.F32  v4, v1, v2
REDUCE.F64 r8, object72
CALL      object9137
RETURN    r8
```

Ja ennen kaikkea se voisi tuntea suoraan meidän korkeamman tason objektit:

```text
TABLE_FILTER
IMAGE_RESIZE
HASH
SORT
MATRIX_MUL
```

joiden alapuolella hardware/runtime päättää varsinaisen konekoodin.

---

# Tässä graafin hyöty tulee uudelleen näkyviin

Oletetaan:

\[
D=(A+B)\times C
\]

Lineaarinen konekoodi pakottaa jonkin järjestyksen:

```asm
load A
load B
add
load C
mul
```

Mutta itse ongelmassa ei ole alun perin tätä IP-järjestystä.

Siinä on vain riippuvuudet:

```text
A ─┐
   ADD ─┐
B ─┘    MUL
C ──────┘
```

Tämä kertoo:

> `ADD` täytyy tehdä ennen `MUL`:ia, koska MUL tarvitsee sen tuloksen.

Mutta jos olisi:

\[
D=(A+B)\times(C+E)
\]

graafi:

```text
A ─┐
   ADD1 ───┐
B ─┘       │
           MUL
C ─┐       │
   ADD2 ───┘
E ─┘
```

kertoo automaattisesti:

\[
ADD1 \parallel ADD2
\]

Ne voidaan suorittaa rinnakkain.

Lineaarisessa assemblerissa AI:n täytyy itse valita järjestys:

```asm
add1
add2
mul
```

vaikka fyysinen superskalaarinen CPU voikin sitten taas jälkikäteen huomata, että `add1` ja `add2` ovat riippumattomia ja ajaa ne rinnakkain.

Huomaat hieman huvittavan kierroksen:

\[
\text{luonnollinen riippuvuusgraafi}
\rightarrow
\text{lineaarinen konekoodi}
\rightarrow
\boxed{\text{CPU rakentaa runtime:ssa riippuvuuksia takaisin}}
\]

Moderni out-of-order x86-CPU tekee juuri tätä: dekoodaa käskyvirran sisäisiksi mikro-operaatioiksi ja suorittaa riippumattomia operaatioita rinnakkain.

Eli IP antaa **arkkitehtonisen järjestyksen**, mutta fyysinen suoritin ei välttämättä suorita kaikkia käskyjä siinä järjestyksessä.

---

## Tässä tullaan kiinnostavaan vaihtoehtoon

Jos todella suunnittelisimme AI-native tietokoneen raudasta asti, voisi kysyä:

> Miksi AI tuottaa lineaarista x64-koodia, jonka CPU sitten purkaa uudelleen riippuvuusgraafiksi?

Voitaisiin antaa raudalle suoremmin:

```text
node 1: ADD A,B -> X
node 2: ADD C,E -> Y
node 3: MUL X,Y -> Z
```

ja hardware scheduler suorittaisi solmun heti, kun sen syötteet ovat valmiita.

Tätä kutsutaan yleisesti **dataflow-arkkitehtuuriksi**.

Se eroaa klassisesta von Neumannista:

### Von Neumann

\[
IP \rightarrow instruction \rightarrow next\ IP
\]

### Dataflow

\[
\boxed{\text{operands ready}}
\rightarrow
\boxed{\text{execute operation}}
\]

Eli juuri tällaisessa koneessa ohjelman graafi voisi olla myös paljon lähempänä todellista suoritusmallia.

---

Mutta nykyisellä PC-raudalla tekisin kompromissin:

\[
\boxed{\text{AI:n semanttinen operaatio}}
\rightarrow
\boxed{\text{SSA/dataflow IR}}
\rightarrow
\boxed{\text{x64 machine code}}
\]

ja sallisin AI:n generoida myös suoraan x64-koodia, kun siitä on hyötyä.

LLVM:n ja MLIR:n kaltainen teknologia osoittaa jo, että korkean tason dataflow-rakenne voidaan laskea vähitellen target-spesifiseksi konekoodiksi; MLIR on nimenomaan suunniteltu esittämään sekä korkean tason dataflow-graafeja että alempaa target-kohtaista koodia. 

Eli **x64 ei ole väärä ajatus lainkaan**. Se on vain mielestäni AI:n *backend*, ei optimaalinen *thinking language*. AI voi aivan hyvin "ymmärtää" `ADD`, `MOV`, `JMP`, `CALL` yhtä luonnollisesti kuin mitä tahansa muita diskreettejä operaatioita, mutta sen ei kannata joutua ratkaisemaan rekisteriallokointia ja AVX-tail-looppia joka kerta, kun varsinainen ongelma on vain `SUM(A)`.
