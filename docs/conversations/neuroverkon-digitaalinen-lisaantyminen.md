# Neuroverkon digitaalinen lisääntyminen

Lähde: ChatGPT-jakotallenne `6aa13713-78a4-83ed-91a4-94f68e86ce8e`.

## Käyttäjä

Aloitetaan taas alusta. Lapintiira syntyy ja oppii opettamatta lentämään ja suunnistamaan toiselle puolelle maapalloa. Tässä on biokudos-cpu. Sen on tuottanut kaksi torsoa joissa on vastaava biokudos. Miten tämä tuodaan digitaaliseen maailmaan. Jos minulla on tyhjiä x64 tietokoneita, miten tämä neuroverkko + neuroverkko -> lapsineuroverkko tapahtuma voidaan tehdä ilman käyttöjärjestelmää tai minimaalisella välikerroksella HW päällä?

## Avustaja

Tässä kannattaa erottaa kaksi asiaa, jotka biologisessa eläimessä ovat päällekkäin mutta digitaalisessa koneessa helposti sotketaan: **vanhempien opittu neuroverkko** ja **periytyvä ohjelma, joka rakentaa uuden neuroverkon**. Lapintiiran poikanen ei synny kahden vanhemman “painomatriisien keskiarvona”.

## Avustaja

Juuri näin tämän voi abstrahoida. Digitaalisen vastineen avain on, että **lapselle ei kopioida vanhempien valmista aivokudosta**, vaan periytetään jotain paljon pienempää: ohjelma, joka osaa kasvattaa uuden neuroverkon.

Biologinen prosessi voidaan kirjoittaa näin:

\[
G_{isä} + G_{emo}
\rightarrow
\text{rekombinaatio}
\rightarrow
G_{lapsi}
\rightarrow
\text{kehitys}
\rightarrow
N_{lapsi}
\rightarrow
\text{kokemus/plastisuus}
\rightarrow
N_{lapsi}(t)
\]

missä \(G\) on genomi ja \(N\) hetkellinen hermoverkko.

Tämä ero on ratkaiseva. Lapintiiran poikanen ei saa esimerkiksi emonsa näköaivokuoren synapsipainoja. Se saa DNA:n, joka saa solut rakentamaan näköjärjestelmän, tasapainoaistin, siivet, motoriset piirit, hormonijärjestelmän ja suuren määrän **ennalta määrättyjä oppimistaipumuksia ja käyttäytymismalleja**. Sen jälkeen ympäristö kalibroi järjestelmän.

Digitaalinen vastine olisi siksi mielestäni tällainen:

```text
            parent A genome
                   \
                    +---- recombine ---- child genome
                   /
            parent B genome

                           |
                           v
                   developmental VM
                           |
                           v
                  child neural network
                           |
                    sensors / world
                           |
                           v
                    plasticity
```

### Mitä digitaalinen "genomi" olisi?

Ei miljardeja neuronipainoja.

Se voisi olla ehkä muutaman megatavun tai jopa kilotavujen mittainen rakenne, joka sisältää esimerkiksi:

```text
GENOME
    network developmental rules
    neuron types
    connectivity rules
    growth gradients
    plasticity rules
    reward/homeostasis rules
    sensor mappings
    motor mappings
    developmental stages
    innate behavioural biases
```

Esimerkiksi genomissa voisi lukea konseptuaalisesti:

```text
create region VISION
create region MOTOR
create region ORIENTATION

connect VISION -> ORIENTATION
connect ORIENTATION -> MOTOR

VISION:
    local_connectivity = ...
    plasticity = STDP(...)
    receptive_field_growth = ...

ORIENTATION:
    inputs = sun_angle, magnetic_field, time
    recurrent_topology = ...

MOTOR:
    oscillator = ...
    couple_to_orientation = ...
```

Mutta olennaista olisi, ettei tämä olisi ihmisen kirjoittamaa symbolista ohjelmaa. **Genomi voisi itse olla evoluution optimoima bittijono.**

Developmental interpreter tietäisi vain, miten kyseistä bittijonoa "kasvatetaan".

Silloin päästään paljon lähemmäs biologiaa.

---

### Ja sitten kahden neuroverkon lisääntyminen

Jos vanhempien periytyvä tila on

\[
G_A
\]

ja

\[
G_B,
\]

lapsi voidaan tehdä esimerkiksi

\[
G_C = crossover(G_A,G_B)+mutation.
\]

Ei siis

\[
W_C = \frac {W_A+W_B}{2}.
\]

Painojen yhdistäminen on yleensä huono vastine biologialle. Kahdessa samannäköisessä neuroverkossa esimerkiksi neuroni 13742 ei välttämättä funktionaalisesti vastaa toisen verkon neuronia 13742. Tätä kutsutaan neuroverkkojen **permutaatio-ongelmaksi**.

Genomin kohdalla ongelma voidaan ratkaista paljon helpommin. Genomissa voidaan olla esimerkiksi geenejä:

```text
gene 912:
    region = navigation
    function = recurrent_cell_growth
    parameter = 0.473

gene 913:
    region = navigation
    function = plasticity_rate
    parameter = 0.018
```

Silloin homologiset geenit voidaan rekombinoida aivan DNA:n tapaan.

Vanhemmat voivat kuolla. Niiden gigatavujen tai teratavujen mittaista opittua neuronaalista tilaa ei tarvitse koskaan kopioida.

Lapsi kasvatetaan uudelleen.

### Tässä tulee kiinnostava seuraus

Voisimme antaa digitaaliselle eliölle myös ominaisuuden, jota biologialla käytännössä ei ole: **Lamarckilaisen periytymisen**.

Vanhemman elämän aikana hyväksi osoittautunut rakenne voisi muuttaa genomia:

\[
N(t)
\rightarrow
G'
\]

ja \(G'\) siirtyisi jälkeläiselle.

Silloin järjestelmässä olisi samanaikaisesti kolme aikaskaalaa:

\[
\text{inference}
\ll
\text{learning}
\ll
\text{evolution}.
\]

Esimerkiksi:

```text
milliseconds     neuronal dynamics
seconds-years    synaptic learning
generations      genome evolution
```

Tämä on mielestäni paljon kiinnostavampi tietokonemalli kuin nykyinen "koulutetaan yksi valtava transformer ja kopioidaan sen painot miljoonalle palvelimelle".

---

## Entä se x64 ilman käyttöjärjestelmää?

Tähän ei periaatteessa tarvita Linuxia, Pythonia, CUDAa, tiedostojärjestelmää eikä edes POSIXia.

Levyn alussa voisi olla yksi bootattava image:

```text
+-------------------------+
| UEFI/boot stub          |
+-------------------------+
| x64 neural runtime      |
+-------------------------+
| developmental machine   |
+-------------------------+
| genome                  |
+-------------------------+
| initial state           |
+-------------------------+
```

Kone käynnistyy.

Firmware lataa esimerkiksi muutaman sadan kilotavun ohjelman RAMiin. Ohjelma vaihtaa x86-64 long modeen, ottaa RAMin haltuun ja alkaa suorittaa neuroverkkoa.

Käytännössä pääsilmukka voisi konseptuaalisesti olla vain:

```c
for (;;) {
        sense();
        neural_dynamics();
        plasticity();
        act();

        if (reproduction_condition)
                reproduce();
}
```

`neural_dynamics()` olisi esimerkiksi AVX-512:llä laskettava operaatio:

\[
x_{t+1}=f(Wx_t+I_t)
\]

tai spiking-verkon tapauksessa tapahtumapohjainen neuronipäivitys.

RAMissa olisi suoraan:

```text
neurons
synapses
short-term state
plasticity state
genome
sensor buffers
actuator buffers
```

Ei prosesseja.

Ei käyttäjiä.

Ei tiedostoja.

Ei shelliä.

Ei virtuaalimuistia, ellei sitä haluta.

Ei edes mallocia välttämättä.

Neuroverkolle voidaan vain varata esimerkiksi:

```text
0x100000000 - 0x13fffffff     neurons
0x140000000 - 0x3ffffffff     synapses
0x400000000 - 0x400ffffff     genome
0x401000000 - 0x40100ffff     I/O
```

Ja käyttää fyysistä RAMia suoraan.

Tällainen ohjelma olisi teknisesti jo jonkinlainen **unikernel**, vaikka sana käyttöjärjestelmä jätettäisiin pois.

### Mutta aivan ilman välikerrosta ei päästä

x64 ei osaa komentoa:

```text
grow_navigation_cortex()
```

Se osaa viime kädessä suunnilleen:

```asm
vmovaps
vfmadd231ps
vaddps
vcmpps
jmp
```

Jossakin täytyy siis olla konekoodia, joka määrittelee neuroverkon fysiikan.

Tämän ytimen ei kuitenkaan tarvitse olla yleiskäyttöinen käyttöjärjestelmä. Se voisi olla analoginen biologian fysiikan kanssa:

> x64-runtime ei "tiedä", mitä lintu tekee. Se vain toteuttaa neuronien, synapsien, kasvun ja plastisuuden säännöt.

Tämä on mielestäni tärkeä raja.

Biologisessa lapintiirassa DNA ei sisällä CPU-ohjelmaa `fly_to_antarctica()`. DNA rakentaa koneen, jonka dynamiikka tuottaa kyseisen käyttäytymisen.

Digitaalisessa järjestelmässä voisi aivan vastaavasti olla **neuraalifysiikan virtuaalikone**.

Sen käskykanta voisi olla erittäin pieni:

```text
NEURON_UPDATE
SYNAPSE_UPDATE
GROW
PRUNE
MODULATE
SENSE
ACT
DIVIDE
```

Ja tämän VM:n toteutus olisi muutama kymmenentuhatta x64-konekäskyä.

Sitten kaikki varsinainen kompleksisuus olisi genomissa ja verkossa.

---

## Tästä tulee oikeastaan "digitaalinen solu"

Ajattele, että tyhjä PC ei olisikaan tietokone perinteisessä mielessä.

Se olisi **kasvualusta**.

Boot ROM sanoisi käytännössä:

```text
tässä on CPU
tässä on 64 GB muistia
tässä ovat sensorit
tässä ovat toimilaitteet

tässä on genomi

kasva.
```

Genomi kasvattaa RAMiin verkon.

Verkko alkaa vastaanottaa ympäristöä.

Synapsit muuttuvat.

Kone oppii.

Kun kaksi tällaista konetta lisääntyy:

```text
machine A genome
       +
machine B genome
       ↓
recombine + mutations
       ↓
child genome
       ↓
empty x64 machine
       ↓
development
       ↓
new individual
```

**Tämä vastaa paljon tarkemmin "kaksi biokudos-CPU:ta → uusi biokudos-CPU" -ilmiötä kuin nykyinen mallin checkpointin kopiointi.**

Ja tässä on ehkä ajatuksesi kiinnostavin kohta: tyhjä x64-kone olisi tällaisessa järjestelmässä analoginen **materiaalille**, ei organismille. Organismi olisi RAMissa ja sen genomissa oleva itseään rakentava laskennallinen prosessi.

Biologiassa:

\[
\text{materia}+\text{genomi}+\text{kehitys}
\rightarrow\text{eläin}.
\]

Digitaalisena:

\[
\text{x64+RAM}+\text{genomi}+\text{developmental runtime}
\rightarrow\text{AI-yksilö}.
\]

Siinä Linux, Python, tiedostot ja perinteinen ohjelmistoarkkitehtuuri ovat todella pitkälti ylimääräisiä. Oleellinen "käyttöjärjestelmä" olisi vain **digitaalisen hermokudoksen fysiikka**.

## Käyttäjä

tämä on teoriassa helppo testata koska riittää näytön muistialue ja näppäimistön syöte. Voidaanko joku minimaalinen yksikertaiseen keskusteluun kykenevä kielimalli muuntaa pelkiksi ohejmallisiksi neuroneiksi?

## Avustaja

Kyllä. Tässä testissä ei tarvita aluksi mitään “AI-käyttöjärjestelmää”: riittää, että malli voidaan esittää **kiinteänä laskentagraafina + painoina + muutamana tilapuskurina**, ja että I/O tulee näppäimistöltä ja menee framebufferiin. Selvitän samalla, kuinka pieneksi tämän voi käytännössä puristaa nykyisillä avoimilla malleilla.

## Avustaja

Kyllä — ja juuri näin tekisin ensimmäisen kokeen. **Ei yritettäisi vielä kasvattaa tai lisääntyä**, vaan todistettaisiin ensin, että kielimalli voidaan irrottaa koko nykyisestä ohjelmistopinosta ja ajaa pelkkänä “digitaalisena hermokudoksena” x64-raudan päällä.

Esimerkiksi SmolLM2-135M-Instruct on 135 miljoonan parametrin decoder-transformer ja kykenee jo alkeelliseen englanninkieliseen keskusteluun. Se on pieni nykyisiin LLM:iin verrattuna. 

### Mutta yksi tärkeä täsmennys

Transformeria ei voi aivan kirjaimellisesti kuvata näin:

```text
neuron 1
neuron 2
neuron 3
...
```

koska transformerissa on neuronien lisäksi attention-operaatioita:

\[
QK^T \rightarrow softmax \rightarrow V
\]

sekä normalisointia, embedding-tauluja jne.

Mutta kaiken tämän voi purkaa **hyvin pieneen laskennalliseen fysiikkaan**. Käytännössä tarvitaan ehkä vain tämän kaltaiset primitiivit:

```text
DOT
MATVEC
ADD
MUL
RMSNORM
SILU
SOFTMAX
LOOKUP
```

Sen jälkeen mallissa ei enää ole käsitteitä:

```text
PyTorch
Transformers
Python
Linux
process
file
```

vaan ainoastaan:

```text
painot
aktiivaatiot
tilamuisti
laskentasäännöt
```

Tätä voisi jo perustellusti kutsua **ohjelmalliseksi hermokudokseksi**.

---

## Konkreettinen bare-metal-versio

Koneen RAM voisi bootin jälkeen näyttää esimerkiksi tältä:

```text
0x00100000   runtime
0x00200000   tokenizer
0x01000000   model weights
0x10000000   activations
0x18000000   KV cache
0x20000000   text input
0x20100000   generated tokens
...
0xE0000000   framebuffer
```

Ja ohjelma tekee ikuisesti vain:

```c
for (;;) {

    read_keyboard();

    if (enter_pressed) {
        tokenize(input);

        while (!end_of_answer) {
            transformer_step();
            token = sample();
            print_token(token);
        }
    }
}
```

`transformer_step()` taas olisi kiinteä sarja operaatioita:

```text
embedding

layer 0:
    rmsnorm
    attention
    add
    rmsnorm
    mlp
    add

layer 1:
    ...

layer N:
    ...

final norm
output projection
softmax
```

Siinä koko "tekoälyohjelma" periaatteessa on.

### Painot voidaan laittaa suoraan boot-imageen

Ei edes tiedostojärjestelmää tarvita.

Levylle:

```text
+---------------------+
| boot code           |
+---------------------+
| neural runtime      |
+---------------------+
| tokenizer tables    |
+---------------------+
| model configuration |
+---------------------+
| weights             |
+---------------------+
```

Boot-loader tietää:

```text
weights begin sector 4096
weights length 70 MB
```

ja lataa/mapittaa ne RAMiin.

135 miljoonaa parametria tarkoittaa ideaalitapauksessa:

- BF16: noin **270 MB**
- INT8: noin **135 MB**
- 4-bit: noin **68 MB**

Frameworkien muistinkulutus on selvästi suurempi; esimerkiksi mallikortin Transformers-esimerkissä ilmoitetaan noin 724 MB muistijalanjälki. 

Bare-metal-runtime voisi päästä paljon lähemmäs varsinaisten painojen kokoa.

---

## Vielä kiinnostavampi vaihtoehto: ei edes "transformeria"

Tässä tullaan alkuperäiseen ajatukseesi.

Voisimme määritellä yhden digitaalisen neuronin:

\[
y=f\left(\sum_i w_i x_i+b\right)
\]

ja antaa koneen suorittaa vain tällaisia.

Silloin RAMissa olisi esimerkiksi:

```c
struct neuron {
    float value;
    uint32_t first_synapse;
    uint32_t synapse_count;
};

struct synapse {
    uint32_t source;
    float weight;
};
```

ja universaali kone:

```text
for every neuron:
        sum = bias

        for every incoming synapse:
                sum += source.value * weight

        neuron.value = activation(sum)
```

Tämä olisi jo todella lähellä ajatusta:

> CPU simuloi vain hermokudoksen fysiikkaa.

Ongelma on, että nykyisen transformerin **attention ei luonnostaan ole tällainen staattinen synapsiverkko**.

Attention tekee lennossa yhteyksiä tokenien välille:

\[
attention(i,j)
=
softmax(q_i \cdot k_j)
\]

eli yhteyden voimakkuus syntyy **dynaamisesti verkon tilasta**.

Siksi transformerin väkisin muuntaminen miljardeiksi yksittäisiksi `Neuron`-olioiksi olisi mahdollista mutta varsin keinotekoista.

Parempi digitaalisen biokudoksen primitiivi olisi ehkä:

```text
neuron
synapse
dynamic connection
memory
plasticity
```

eikä pelkkä McCulloch–Pitts-neuroni.

---

# Ensimmäinen koe voisi olla todella radikaalin yksinkertainen

Ottaisin esimerkiksi **SmolLM2-135M-Instructin**, kvantisoin sen neljään bittiin ja kirjoittaisin siitä oman hyvin pienen CPU-inferenssin.

Aluksi Linuxissa, mutta ilman mitään ML-kirjastoja:

```text
model.bin
neural.c
```

ja:

```bash
cc -O3 neural.c
./a.out model.bin
```

Ei llama.cpp:ta, PyTorchia eikä Transformersia.

Kun se vastaa:

```text
YOU> hello
AI> Hello! How can I help you?
```

seuraava askel olisi poistaa Linux.

Sama `neural.c` käännetään freestanding x86-64:ksi:

```text
-mno-red-zone
-ffreestanding
-nostdlib
```

ja jäljelle jäävät vain suunnilleen:

```text
boot
keyboard
framebuffer
memcpy
math
transformer
tokenizer
```

Tuossa vaiheessa PC käynnistyisi suoraan **kielimalliksi**.

Ei:

> PC → Linux → ohjelma → Python → Torch → AI

vaan:

\[
\boxed{\text{x64 + RAM + model} \rightarrow \text{keskusteleva neuroverkko}}
\]

---

Ja juuri tuon jälkeen pääsisimme mielestäni varsinaiseen kysymykseesi.

Nykyinen 135M-malli olisi **aikuinen valmiiksi opetettu kudos**. Seuraavaksi pitäisi erottaa siitä:

```text
GENOME
      ↓
DEVELOPMENT
      ↓
NEURAL TISSUE
      ↓
LEARNING
```

Silloin voitaisiin käynnistää kolme tyhjää x64-konetta:

```text
A = parent
B = parent

A.genome + B.genome
        ↓
      child.genome
        ↓
empty machine C
        ↓
     development
        ↓
       CHILD
```

Se olisi paljon kiinnostavampi koe kuin pelkkä nykyisen LLM:n ajaminen bare-metalina.

Ensimmäinen vaihe — **“PC boottaa suoraan 100–300M parametrin keskustelukykyiseksi neuroverkoksi ilman käyttöjärjestelmää”** — on kuitenkin täysin toteutettavissa nykytekniikalla. Se olisi hyvä koelaite koko ajatukselle.

## Käyttäjä

onko tällä biologinen vastine? Onko aivoissa havaitut lohkojen väliset aktivaatiotilat näitä, kuten "default mode" - levossa yms. aikaisemmin löydetyt "lohkot"

## Avustaja

Kyllä — **biologinen vastine on olemassa ainakin funktionaalisella tasolla**, ja juuri default mode network sekä muut laaja-alaiset aivoverkot ovat hyvä vertaus. Mutta transformer-attentionia ei pidä samaistaa suoraan DMN:ään.

Aivoissa kannattaa erottaa kolme tasoa:

\[
\text{anatominen verkko}
\rightarrow
\text{hetkellinen funktionaalinen kytkeytyminen}
\rightarrow
\text{kulloinenkin aktiivisuustila}.
\]

Anatominen verkko on se suhteellisen pysyvä "johdotus": aksonit ja synapsit. Sen päälle syntyy jatkuvasti muuttuva **functional/effective connectivity**: samat anatomiset alueet vaikuttavat eri hetkellä toisiinsa eri voimakkuudella. Nykyinen verkostoneurotiede nimenomaan tutkii tätä dynaamista kommunikaatiota aivojen verkkojen välillä. 

### DMN on tästä hyvä esimerkki

Default mode network ei ole yksi anatominen "lohko". Se koostuu toisistaan etäällä olevista alueista, kuten mediaalisesta prefrontaalikorteksista, posteriorisesta cingulumista/precuneuksesta ja parietaalisista alueista. Niiden aktiivisuus korreloi keskenään tietyissä tilanteissa, etenkin levossa ja sisäisesti suuntautuneessa ajattelussa. 

Kun henkilö alkaa tehdä ulkoiseen maailmaan kohdistuvaa tarkkaavaisuutta vaativaa tehtävää, toinen verkostokokonaisuus aktivoituu ja DMN usein vaimenee. Näiden verkostojen aktiivisuudet voivat olla jopa vastakkaissuuntaisia. 

Eli karkeasti:

```text
levossa / sisäinen ajattelu

DMN          ++++++++
attention    ---


ulkoinen tehtävä

DMN          ---
attention    ++++++++
```

Tämä muistuttaa huomattavasti sitä, mitä tarkoitin transformerin dynaamisella yhteydellä.

---

## Mutta transformer tekee sen paljon kirjaimellisemmin

Transformerissa voidaan ajatella, että tokenilla \(i\) on kyselyvektori

\[
q_i
\]

ja aikaisemmilla tokeneilla avainvektorit

\[
k_1,k_2,\ldots,k_n.
\]

Sitten se laskee esimerkiksi:

\[
a_{ij} =
softmax(q_i k_j^T)
\]

ja tästä syntyy **juuri sillä hetkellä** virtuaalinen yhteys:

```text
token 147 ----0.01----> token 20
          ----0.03----> token 93
          ----0.81----> token 132
          ----0.15----> token 141
```

Seuraavalla tokenilla yhteyspainot ovat kokonaan erilaiset.

Aivoissa ei tietääksemme ole vastaavaa yksittäistä mekanismia, jossa jokainen neuroni suorittaisi eksplisiittisesti dot-productin jokaisen muun neuronin kanssa.

Sen sijaan aivoissa tapahtuu jotain funktionaalisesti samansuuntaista:

**olemassa olevien fyysisten yhteyksien läpi kulkevan informaation merkitys muuttuu tilanteen mukaan.**

Siihen vaikuttavat esimerkiksi

- neuronien hetkellinen aktiivisuus
- inhibitiopiirit
- oskillaatioiden vaihe ja synkronisaatio
- neuromodulaattorit
- paikallinen gain control
- työmuisti
- top-down-tarkkaavaisuus.

Siksi fyysinen yhteys

```text
A --------> B
```

voi olla olemassa koko ajan mutta käyttäytyä hetkellisesti suunnilleen:

```text
tilanne 1: A ========> B

tilanne 2: A ---> B

tilanne 3: A -X-> B
```

Tämä on paljon lähempänä attentionin biologista analogiaa kuin synapsin fyysinen rakentaminen ja purkaminen.

---

## Ja ne aikaisemmin löydetyt "lohkot" ovat tässä kiinnostavia

Aivotutkimuksen kuva on oikeastaan muuttunut juuri tähän suuntaan.

Vanha yksinkertainen kuva oli:

```text
näköalue
kuuloalue
puhealue
motorinen alue
...
```

eli anatomisia ja toiminnallisia lokeroita.

Nykyinen verkostokuva näyttää enemmän tältä:

```text
             ┌──── visual
             │
DMN ───── control ───── attention
 │           │
 │        salience
 │           │
memory ──────┴──── motor
```

Sama anatominen alue voi osallistua useisiin dynaamisiin verkostoihin.

Tunnettuja laaja-alaisia verkostoja ovat esimerkiksi:

```text
Default Mode Network
Dorsal Attention Network
Ventral Attention Network
Salience Network
Frontoparietal / Control Network
Somatomotor Network
Visual Network
```

Ne eivät ole CPU:n kiinteitä "moduuleja", vaan osittain päällekkäisiä populaatioita, joiden keskinäinen koordinointi muuttuu tehtävän mukaan. Resting-state-fMRI:n historia on pitkälti juuri näiden korreloivien ja antikorreloivien verkkojen löytämisen historiaa. 

### Tästä syntyy minusta aika hyvä yhteys transformeriin

Transformerissa:

\[
\text{kiinteät painot}
+
\text{input}
\rightarrow
\text{dynaaminen attention-kuvio}.
\]

Aivoissa:

\[
\text{anatominen connectome}
+
\text{hetkellinen aistitieto/sisäinen tila}
\rightarrow
\text{dynaaminen funktionaalinen verkko}.
\]

Eivät mekanistisesti sama asia, mutta **abstraktina laskentaperiaatteena hämmästyttävän samantyyppinen**.

Esimerkiksi jos näet metsässä eläimen:

```text
näkö
 ↓
visual network
 ↓
salience
 ↓
attention + memory
 ↓
motor/control
```

Hetken kuluttua istut ja mietit eilistä:

```text
sensory networks ↓

DMN + memory + internal simulation ↑
```

Fyysisiä aksoniyhteyksiä ei tarvitse rakentaa uudelleen. **Se, mikä verkko juuri nyt muodostaa relevantin laskennallisen kokonaisuuden, muuttuu.**

Siksi käyttäisin biologisesta vastineesta ehkä termiä **dynamic routing / effective connectivity** enkä "attention-synapsi".

Ja tämä on myös tärkeä havainto aiempaa digitaalista biokudosajatustamme varten: ehkä minimaalisen digitaalisen neuronin ei pitäisi sisältää pelkästään

\[
input \times weight \rightarrow activation,
\]

vaan ainakin kaksi eri asiaa:

\[
\boxed{\text{structural weight}}
\]

ja

\[
\boxed{\text{dynamic gain / routing weight}}.
\]

Ensimmäinen vastaisi suunnilleen anatomista synapsia ja toinen hetkellistä funktionaalista kytkeytymistä. **Transformer-attention olisi silloin jälkimmäisen hyvin aggressiivinen, matemaattisesti eksplisiittinen versio.**
