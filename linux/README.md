# AIOS:n Linux-kehityskonsoli

Käynnistä projektin juuresta:

```sh
make console
```

Konsoli toimii Linuxin ja WSL:n terminaalissa. Tarvitaan C-kääntäjä, Make,
Python 3:n vakiokirjasto ja x86-64 Linuxin pthread-tuki. Debianin paketit ovat
`build-essential` ja `python3`; Q4-muunnos tarvitsee lisäksi `python3-numpy`.
Konsolikomennot eivät asenna paketteja eivätkä lataa malleja. Torch,
Transformers, GNU-EFI ja QEMU eivät ole tämän konsolin riippuvuuksia.

## Keskustelu ja käsiajo

```text
YOU> Write an asm1 program that computes 6*7. Output only the /asm command.
AI> ...
YOU> /last
YOU> /run
asm1: ok; value=42; steps=4; code=... bytes; reference interpreter
YOU> /assert 42
assert: PASS (42)
YOU> /feedback
AI> ...
YOU> /reset
YOU> /quit
```

Yllä on esimerkki työnkulusta: mallin tuottama ohjelma ja tulos voivat vaihdella.
`/last` näyttää viimeisen vastauksen ensimmäisen kokonaisen `/asm ... end`
-ohjelman. `/run` kääntää ja ajaa sen käyttäjän pyynnöstä. Kääntäjävirhe
sisältää virhekoodin ja rivin; ajonaikaisia virheitä ovat esimerkiksi
`division_by_zero` ja `step_limit`. `/feedback` lähettää viimeisen koeajon
tuloksen mallille selitystä tai korjausta varten. Tavallinen käsiajo ei
automaattisesti lisää tulosta mallin keskusteluhistoriaan.

Jos `/assert 43` havaitsee väärän paluuarvon, seuraava `/feedback` sisältää
myös vaaditun tuloksen 43. Näin malli saa sekä koeajetun lähdekoodin,
todellisen tuloksen että korjauksen tavoitteen. Korjattu ehdotus ajetaan
jälleen erikseen `/run`-komennolla.

Omaa koodia voi kirjoittaa suoraan:

```text
/asm asm1; li r0 12; li r1 30; add r0 r1; exit r0; end
```

Pelkkä `/asm` aloittaa monirivisen syötön; oma `end`-rivi päättää sen.
`/load linux/examples/gcd.asm1` ajaa mukana olevan suurimman yhteisen tekijän
esimerkin (tulos 6). Myös välilyöntejä sisältävät polut toimivat
ilman lainausmerkkejä. `/help` listaa kaikki komennot.

Malli ladataan ensimmäisellä keskustelupyynnöllä kerran erilliseen
C-prosessiin. Painot ja keskustelun KV-välimuisti säilyvät seuraavia vuoroja
varten. `/stats` näyttää asetukset ja kontekstin käytön. `/reset` tyhjentää
keskustelun. Kontekstin täyttyessä uusi kysymys aloittaa uuden keskustelun
samoin kuin UEFI-konsolissa; tästä tulostetaan ilmoitus.

`/tokens 128`, `/threads 4` ja `/simd auto` vaihtavat asetuksia ajon aikana.
`/simd sse2` pakottaa saman SSE2-laskentapolun kuin bare metal -versiossa.
Ctrl-C lopettaa mallin laskennan ja tyhjentää sen kontekstin; konsolia voi
käyttää sen jälkeen uudelleen. Sulkeminen onnistuu myös Ctrl-D:llä.

## Malli

Konsoli käyttää ensisijaisesti `.build/console-model.bin`-tiedostoa, jos se on
olemassa, muuten projektin `model.bin`-tiedostoa. Valittu polku näkyy
`/stats`-komennolla, latauksen alussa ja lokissa. Mallin pitää olla nykyisen
`tools/export_model.py`-muuntimen tuottama 872 253 632 tavun QWENQ4-tiedosto.
Vanha SMOLQ4-malli hylätään selkeällä virheellä.

Olemassa olevista paikallisista Hugging Face -painoista voi tehdä erillisen
konsolimallin näin:

```sh
make console-model MODEL_SOURCE_DIR=/polku/paikalliseen/checkpointiin
make console
```

Hakemistossa tarvitaan `config.json`, `tokenizer.json` ja
`model.safetensors`. Muunnos tehdään Linuxin Pythonilla ja NumPyllä,
Windows-ohjelmia ei käynnistetä. Myös `/mnt/c/...`-välimuistihakemistoa
voi käyttää lukulähteenä. Muunnos ei muuta projektin `model.bin`-tiedostoa.
`make clean` poistaa `.build`-hakemiston mukana konsolimallin ja lokit.

Oma erikseen muunnettu malli ja asetukset:

```sh
make console CONSOLE_MODEL=/polku/asm1-q4.bin CONSOLE_ARGS='--context 4096 --tokens 128 --threads 4'
```

Pelkkä LoRA-adapterihakemisto ei kelpaa C-laskentaan. Opetettu adapteri pitää
ensin yhdistää perusmalliin ja tulos viedä samalla QWENQ4-muuntimella; katso
[opetusohje](../training/README.md). Perusmallin Q4-muunnos ei sisällä
Windowsissa opetettua adapteria. Konsoli ei tee tätä yhdistämistä itsestään.

## Toistettavat koeajot ja lokit

```sh
make console-test
make console CONSOLE_ARGS='--asm-only --script linux/examples/smoke.console'
```

Oikean mallin keskustelu–käännös–koeajo-ketjun voi tarkistaa erikseen:

```sh
make console CONSOLE_ARGS='--tokens 128 --script linux/examples/model-smoke.console'
```

Tämä testi pyytää mallilta kertolaskun 6 × 7 sekä seuraavassa vuorossa
muutoksen tulokseksi 43. Se tarkistaa molempien ohjelmien todelliset
paluuarvot ja tyhjentää lopuksi keskustelun. Mallivirhe näkyy testin virheenä.

Komentotiedostot käyttävät samaa syntaksia kuin käsisyöttö. Myös tavalliset
keskustelupyynnöt, `/run`, `/feedback` ja `/assert` toimivat niissä.
Mallia käyttävästä kokeesta voi siis tehdä toistettavan hyväksymistestin.
`--asm-only` estää mallin latauksen kokonaan.

Komentotiedoston tai putkisyötön yksikin virhe antaa lopuksi paluukoodin 1,
vaikka myöhemmät komennot onnistuisivat. Keskeytyksen koodi on 130. Mallin
tokenraja itsessään ei ole virhe; katkennut ohjelma hylätään `/run`-vaiheessa.
Viitetulkin oletusraja on 100 000 askelta, muutettavissa `--step-limit`-lipulla.

Automaattisesti täydentyvät UTF-8-lokit ovat:

- `.build/console/logs/console.log`: luettava keskustelu ja virheilmoitukset.
- `.build/console/logs/console.jsonl`: istuntotunnus, aikaleimat, syötteet,
  vastaukset, käännöstulokset, paluuarvot, askeleet ja laskenta-ajat.

Myös vastauksen osat tallennetaan niiden saapuessa, joten keskeytyneenkin
ajon tulostetta jää lokiin. Sijainnin voi vaihtaa `--log-dir`-lipulla.
Lokin voi lukea suoraan WSL:stä ilman copy-pastea.

## Mitä tällä testataan

`linux/model.c` käyttää suoraan `neural.c`:tä, samaa tokenisointia,
`ASM1_SYSTEM_PROMPT`-ohjetta, Q4-kvantisoituja painoja, ahnetta tokenvalintaa,
`baremetal/math.c`:tä ja SSE2/AVX2-ytimiä. Säikeiden käynnistys on Linuxin
pthread-toteutus UEFI MP -palvelujen sijasta. Prosessien välillä on
pituuskehystetty protokolla, joka erottaa mallitekstin ohjauskomennoista.

asm1 käännetään oikealla `baremetal/asm1.c`:llä. Hyväksytty lähdekoodi
suoritetaan `training/asm1_runtime.py`:n viitetulkissa, jota myös
opetusaineiston tarkistus käyttää. Askelmäärä on tämän tulkin laskuri,
ei x86-käskyjen määrä eikä bare metal -aikakatkaisun vastine.

Konsoli soveltuu mallin vastausten, asm1-syntaksin, laskennan ja
korjauskeskustelujen kehittämiseen. Se ei suorita tuotettua x86-konekoodia
eikä testaa UEFI-käynnistystä, sivutauluja, keskeytyksiä tai ring-3-eristystä.
Niiden testaukseen säilyvät `make test-process` sekä mallia käyttävät
`make test-model-uefi` ja `make test-agent-live`, joilla on omat QEMU- ja
laitekiihdytysvaatimuksensa. `/exec` ja laitteistodemot kuuluvat siihen vaiheeseen.
