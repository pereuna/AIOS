# Linux-kehityskonsoli

`make console` käynnistää paikallisen Q4-mallin keskustelukonsolin.
`make console-test` tarkistaa mallityöntekijän ja konsolin ilman mallipainoja.

Komennot: `/help`, `/reset`, `/stats`, `/tokens N`, `/threads N`,
`/simd auto|sse2` ja `/quit`. Lokit ovat `.build/console/logs/`-hakemistossa.
`--script PATH` lukee keskustelun tiedostosta. `--no-model` sallii konsolin
komentojen tarkistamisen ilman mallin lataamista.

Mallin laskenta käyttää samaa `neural.c`:tä kuin UEFI-toteutus. Malli tuottaa
laskuihin LLVM SSA IR -funktion, joka näkyy vastauksena; Linux-konsoli ei käännä
tai suorita sitä eikä palauta CPU:n tulosta mallille. Bare metal -konsolin
`/calc` sekä natiivi + QEMU -arviointi kuvataan [laskinohjeessa](../docs/calc.md).

Paikallisen checkpointin voi viedä Q4-muotoon komennolla `make console-model`.
`CONSOLE_MODEL` valitsee mallitiedoston ja `CONSOLE_ARGS` konsolin argumentit.
