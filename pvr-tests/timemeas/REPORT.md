# PowerVR2 timing: what the ISP, the TSP and the write-out cost

Measured on one Dreamcast (VGA cable) with `timemeas`, 2026-11-16. Times are in **clocks of the core
(100 MHz, 10 ns)** unless a line says counts; the raw tables (in counts of 5 ns) are in
[tables.txt](tables.txt), made by `python3 pvr-tests/timemeas/analyze.py` from the three runs in `fit/timemeas_logs`.

## The model in short

A tile goes through three stages. Stages of consecutive region array entries overlap, so in a render of many
tiles each tile costs about the largest of its three.

| stage | cost of a tile |
|---|---|
| ISP | fixed part, + 20 to 31 for each of the five lists that has a pointer, + **16 per triangle** (strip: 17 on average, sprite: 19), + 1 per line and triangle beyond 16 lines |
| TSP | **1.01 per pixel** of the tile (the background polygon shades whatever nothing else covers: 1035 for an empty tile), but **per triangle seen at least 39** untextured, **48** textured, **64** textured with an offset colour that is not 0 (an offset colour of 0 costs nothing), and **per run of pixels at least about 4.7 / 6 / 8**; one clock more per pixel for a palette with ARGB8888 entries, for YUV422 bilinear and for bilinear without twiddling; more for texels far apart |
| write-out | 1045 with a 16 bit framebuffer, 1105 with 888, 1357 with 0888 or 8888, also when the clip removes every pixel |

One tile alone, nothing in it, pointers to all five lists: 2695 clocks (27.0 µs) from STARTRENDER to the
TSP's end-of-render bit with a 16 bit framebuffer, 3005 with 0888. Of the 0888 tile without its lists,
1355 are the write-out, 1177 the background polygon (1035 of shading, 142 of Z clear and setup) and 335
everything else. An entry with the keep Z bit has no Z clear and no background.

So there is a cost per triangle in the TSP and a cost per span, and the per-triangle one is the large one:
a triangle is free only if its own pixels take longer than its setup (about 39 pixels untextured, 48
textured, 64 with an offset colour that is not 0).

## Method

- The region array has one tile. Interrupts are off, the display is off (it stalls the core for about 3 µs
  whenever it reads a line), and the end-of-render bits of SB_ISTNRM are polled; the CPU's cycle counter
  (5 ns) is read when a bit appears. The poll loop takes 140 ns a turn, so a single reading is late by up to
  14 clocks; every scene is rendered 11 times in each of three runs and the least is taken. Slopes over
  64 to 256 objects repeat to 0.1 clock, single differences to about 5.
- **Only the TSP's bit is used**, which is the end of the tile's write-out. The ISP's bit is not the end of
  the ISP's work (see the last section).
- **ISP**: the background polygon alone, then N objects behind it (background depth 1, objects at 0.5 with
  compare GREATER). The TSP's work is the same in both, so the TSP's bit moves by the ISP's time.
- **TSP**: every scene has a twin with the same polygons behind the background. The difference is what the
  seen polygons cost more than the background they cover; adding the background's time for a tile (B) gives
  the TSP's time for the tile.
- **Modifier volumes** as a third way: the same scene with a volume closed as "inside" and as "outside".
  Punch through polygons inside an opaque modifier volume are shaded a second time, so there the
  difference is the TSP's work alone, and it confirms the twin method. Opaque polygons are not shaded
  twice; there a volume over part of a polygon gives the cost of runs with nothing else changed.
- The background polygon is flat and untextured, the cheapest there is.

## Fixed costs of a tile

**The background polygon is a constant load of the TSP.** Every entry of the region array is a tile to the
TSP, also one that draws nothing and is not written out, as the first entry of KallistiOS's region array:
its background is shaded, 32 runs of 32 pixels. Entries of that kind in front of the tile:

| entries in front | 0 | 1 | 2 | 4 |
|---|---|---|---|---|
| TSP's bit, counts (0888) | 6010 | 7806 | 9890 | 14030 |

Each costs **B = 1035 clocks** (2064 to 2077 counts in different run sets: 1.01 clock a pixel); the first
898, because its ISP part is shorter than the tile's own. This is what I had reported as "the TSP cannot
start before 13.5 µs": it was shading the background of KallistiOS's first entry. The ISP works on the next
entry meanwhile: with one such entry in front, the first 1000 clocks of ISP work on the tile cost nothing
(16 quads: +20 clocks instead of +460; 64 quads: +1123 instead of +2087).

**Lists.** With no list pointers at all the tile takes 2864 (0888); a pointer to an empty list adds 31
(opaque), 30 (opaque modifier), 28 (translucent), 20 (translucent modifier), 30 (punch through).

**Write-out**, tile alone and background only (TSP's bit in clocks), and what each further tile of a
render adds:

| framebuffer | 0555, 565, 4444, 1555 | 888 | 0888, 8888 |
|---|---|---|---|
| one tile | 2695 | 2755 | 3005 |
| each further tile | 1042 | 1096 | 1358 |

Dither changes nothing. With 16 bits the write-out is never the slowest stage (a further tile costs B); with
32 bits it is, at 1358. From that: write-out about 1045 / 1105 / 1358, TSP 1035, and about 470 for the
start, the ISP's fixed part and the hand-overs, plus the lists. The keep Z bit, below, measures these apart.
With the framebuffer clip (0888): 16 of the 32 lines left: 2845; 16 columns, 16x16 or nothing left: 2696.
The write-out's 1045 are spent even when every pixel is clipped; what 32 bits add is per line written and
only when a line is more than 64 bytes.

**The "keep Z" bit of a region array entry** (bit 30) skips the Z clear and the background polygon. All
entries are of tile (0, 0), 0888 framebuffer; the background's colour changes from scene to scene:

| entries | picture | TSP's bit, clocks |
|---|---|---|
| the tile as usual, five empty lists | follows the background's colour | 2998 |
| keep Z, five empty lists | the picture before: nothing is drawn | 1820 |
| keep Z, no list pointer | the picture before | 1690 |
| an entry that draws nothing (not written out), then the tile | | 3897 |
| that entry with keep Z, then the tile | | 3015 |
| four such entries with keep Z, then the tile | | 3069 |

- **The background costs a tile 1177**: its 1035 of shading and 142 before that (the Z clear and its setup).
- **A tile with nothing to do costs 1690**: 1355 of write-out (measured by itself: the same two entries with
  the first written out or not) and **335** for the start, the ISP and the TSP finding nothing, and the
  hand-overs. 335 + 142 + 1035 + 1355 = 2867 is the tile of background without list pointers (measured:
  2859 to 2864).
- An entry that draws nothing costs 899 as KallistiOS writes it and **17 with keep Z**.
- Two entries of the same tile, the second with keep Z (it draws the translucent list's objects through its
  opaque pointer, so that it has other polygons than the first):
  - The depth is kept: a quad behind the first entry's sprite stays hidden. Without keep Z the tile is
    cleared, and the quad shows over a new background.
  - The TSP has two buffers for a tile's colours, and the one a tile is not shaded into is the secondary
    accumulation buffer of the TSP word's select bits (the test accbuf, in the README): while one is
    written to the framebuffer, the next tile is shaded into the other. So the first entry's picture is
    there for the second only if the first is **not written out**: then the second adds to it
    (background from the first, a 16x16 quad from the second, both in the picture, for each of the
    three background colours).
  - If the first is written out and the second shades anything, that goes into the other buffer, which
    still holds what was shaded two write-outs back: the 16x16 quad comes out on the background colour
    of the scene before, the same for all three colours of this one.
  - If the first is written out and the second shades nothing (all of it hidden), the second is not
    written to the framebuffer at all: the cell keeps the first entry's picture. (accbuf: the buffers
    change places, and a tile's write-out starts, when the TSP starts on the next tile; and two keep Z
    tiles in a row that shade nothing hang the render.)
  - The second entry shades only what it draws. With all of it hidden it costs nothing: 2981 for the two
    entries against 2974 for the sprite alone.
- A tile covered by an opaque sprite gains 32 from keep Z: its hidden background was not shaded anyway.

## ISP

Per object, from 64 to 256 of them, by the TSP's bit (clocks):

| object | behind the background | compare NEVER | passing, covered afterwards |
|---|---|---|---|
| triangle 8x8 | 16.0 | 16.0 | 16.0 |
| triangle of half a pixel | 16.0 | 16.0 | 16.0 |
| quad 8x8, 1x1 or 32x1 (strip of two) | 34.0 | 34.0 | 34.0 |
| the same textured, or with offset colour | 34.0 | 34.0 | 34.0 |
| sprite 8x8 or 1x1 | 19.0 | 19.0 | 19.0 |
| strip of 6 triangles | 102.1 | 102.0 | 102.0 |
| quad 32x32 | 64.0 | 64.0 | 64.0 |
| quad 1x32 | 50.0 | 50.0 | 50.0 |
| sprite 32x32 | 32.0 | 32.0 | 32.0 |

- Strips of 1 to 6 triangles: 16, 34, 53, 68, 87, 102: after the first, alternately 19 and 15.
- A triangle culled by its winding or as too small: 15.2.
- Failing, never passing, or passing and writing depth cost the same. So do the vertex format and the place
  in the tile.
- Size matters only beyond 16 lines: a 32 wide quad takes 34 up to 16 lines, then 36, 42, 50, 58, 64 at
  17, 20, 24, 28, 32 lines (2 a line, one per triangle). A 1 wide quad takes 34 up to 20 lines, then 39,
  45, 50 at 24, 28, 32. A sprite 32 tall: 32.
- The first objects do not follow the slope. Totals for N objects behind the background (single
  differences, good to about 10):

| N | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 |
|---|---|---|---|---|---|---|---|---|---|
| triangles | 72 | 100 | 126 | 156 | 288 | 428 | 946 | 1988 | 4010 |
| quads | 101 | 113 | 113 | 189 | 460 | 998 | 2087 | 4272 | 8621 |
| sprites | 82 | 103 | 122 | 180 | 342 | 532 | 1138 | 2368 | 4776 |

  The first object costs 70 to 100, the next few little, and from about 32 on the total is N times the
  slope less 80 to 90 (less 30 to 60 when the tile has three lists instead of five: the first objects' work
  runs beside the tile's fixed part).
- For objects that are not seen the ISP's bit moves exactly as the TSP's does (16.0, 34.0, ... by either).

## TSP

### Per pixel

The background polygon in every format, per region array entry (the same texels in every entry, so these
are texels read before), and what the first tile costs on top:

| format | clocks a pixel | first tile, clocks |
|---|---|---|
| untextured, flat or Gouraud | 1.01 | 0 |
| 565, 1555, 4444, YUV422, bump, point sampled, twiddled or not; offset colour; fog; mipmapped point; trilinear pass B | 1.01 | +80 to +110 |
| VQ: 565 and YUV422, point sampled (565 also mipmapped) | 1.01 | +105 to +135 |
| palette 4 or 8 bpp with **16 bit entries** (ARGB1555, RGB565, ARGB4444), point sampled, also VQ | 1.01 | |
| 565 VQ bilinear | 1.01 | +455 |
| palette bilinear, 16 bit entries | 1.05 (8 bpp), 1.10 (4 bpp) | |
| 565, 1555, 4444 bilinear | 1.11 | +70 to +80 |
| 565 mipmapped bilinear, trilinear pass A | 1.37 | +55 to +65 |
| **palette with ARGB8888 entries**: 4 or 8 bpp, point or bilinear, VQ or not | 2.00 | +80 to +185 |
| **YUV422 bilinear**, twiddled (also mipmapped, also VQ) | 2.00 | +115 (VQ: +355) |
| **bilinear, not twiddled**: 565 and YUV422 | 2.07 | +150 |
| 8 texels a pixel, not mipmapped: 565, YUV422, palette; 565 also bilinear | 3.27 | +80 to +90 |
| 565 VQ, 8 texels a pixel | 3.58 | |
| YUV422 bilinear, 8 texels a pixel | 4.28 | |
| 565 mipmapped, 8 texels a pixel; trilinear A at 6 texels a pixel | 2.14; 2.18 | +65; +35 |

One pixel a clock, whatever the shading. One more clock a pixel is paid in three cases: **a palette whose
entries are ARGB8888** (PAL_RAM_CTRL 3; with 16 bit entries a palette costs nothing, point sampled, and 0.04
to 0.09 bilinear), **bilinear filtering of YUV422**, and **bilinear filtering of a texture that is not
twiddled** (565 or YUV422: 2.07). Texels that are far apart from one pixel to the next cost 2.25 more. Flat
or Gouraud, offset colour and fog cost nothing per pixel. When a sprite covers the tile, the background's
format changes nothing: what is not seen is not shaded. (VQ with YUV422 and VQ with a palette: only the time
was measured, not what such a texture looks like.)

**Texels wider than 16 bits.** The two cases that cost one clock more, looked at with the bilinear footprint's
start fixed: the texture is stepped 1, 2 or 4 texels a pixel in both directions and offset, so that every
pixel's 2x2 footprint starts at an even or at an odd texel (clocks a pixel; whether the footprint's row is
even or odd changes nothing in any line, at most 0.06):

| | 1 texel a pixel | 2, even x | 2, odd x | 4, even x | 4, odd x |
|---|---|---|---|---|---|
| YUV422 bilinear | 2.00 | 2.01 | 2.01 | 2.13 | **3.14** |
| YUV422 point | 1.01 | 1.57 | 1.57 | 2.14 | 2.14 |
| 565 bilinear | 1.11 | 1.57 | 1.86 | 2.14 | 2.14 |
| palette 8 bpp bilinear, ARGB8888 entries | 2.00 | 2.00 | 2.00 | 2.14 | 2.14 |
| palette 8 bpp point, ARGB8888 entries | 2.00 | 2.00 | 2.00 | 2.14 | 2.13 |
| palette 8 bpp bilinear, RGB565 entries | 1.05 | 1.26 to 1.30 | 1.55 to 1.61 | 2.14 | 2.14 |
| palette 8 bpp point, RGB565 entries | 1.01 | 1.26 | 1.26 | 2.14 | 2.14 |

- 4 texels apart, everything is bound by the memory at 2.14, and the extra clock of the 8888 palette and of
  a YUV footprint that starts at an even x is hidden behind that wait.
- **The 8888 palette pays one clock per pixel and nothing else**: no dependence on where the footprint
  starts, point sampled or bilinear. That is what a 16 bit wide path per filter tap would give, with a 32 bit
  entry passing in two steps.
- **YUV422 bilinear costs 2 when the footprint starts at an even x, and 3 when it starts at an odd x and
  shares nothing with the pixel before** (4 texels a pixel: 3.14, also with a step of 1 along y: 3.15). At
  an even x the two texels of a row are one U/V pair, so the four texels' words hold everything; at an odd x
  each texel needs its partner's word as well, eight words. What fits all thirteen numbers: four 16 bit words
  come in per clock, words that came in for the pixel before do not count, and a bilinear YUV pixel takes
  one clock more after its words are in: 1 + 1 at an even x, 1 + 2 at an odd x with nothing shared, 1 + 1 at
  an odd x when the pixels are 1 or 2 texels apart. Point sampled, a YUV texel's two words come in one clock
  and there is no clock more.
- This rules out two converters that work on any two texels (the start would not matter), one converter per
  U/V pair (an odd start would cost 4), and one memory word of four texels per clock (the row's parity would
  matter).

A tile in which polygons are seen costs 55 to 90 more than a tile of background, once, whatever their number
(1 sprite, 4 sprites of 8x32, 16 of 8x8 or 16 of 32x2: 1089 to 1114 against 1035).

### Per triangle

1024 polygons of 1x1 over the tile: every seen triangle costs this, however small (clocks):

| | per seen triangle |
|---|---|
| untextured, flat or Gouraud | 39.0 |
| textured: every format, filter, mipmapped or not, fog | 48.1 |
| textured with offset colour | 64.1 |
| bump map | 49.1 |
| punch through list (565) | 58.0 |
| translucent list, untextured or 565 | 39.0 |

A triangle's own pixels come out while the next one is set up: a triangle takes **the larger of this and
the time of its runs**. Untextured sprites of 1x1 up to 16x1, 8x2 and 4x4 all take 39 to 40; 32x1 takes
40.7; 32x2 (64 pixels) 69.4; 32x32 1102. The 17 polygons you asked for show it directly (difference to the
same polygons unseen):

| 17 polygons, 32x32, then each 2 narrower, and a last one 1 wide | as stripes (1024 runs) | turned by a quarter: rows, one polygon a line |
|---|---|---|
| untextured quads (34 triangles) | +6027 | +339 |
| untextured sprites | +3899 | +85 |
| 565 Gouraud quads | +7688 | +692 |
| 565 sprites | +5130 | +274 |

As rows the quads cost 254 more than the sprites: each quad is two triangles of 32 pixels, which take 39
each where their pixels alone would take 32.5 (a sprite's 64 pixels take their 65). As stripes the same
triangles cost 6000: that is runs.

### Per run of pixels (span)

What one run costs with its pixels, in clocks (its pixels alone: 1.01 each):

| run | untextured | 565 | 565, offset colour |
|---|---|---|---|
| 1 pixel: columns over a quad, up to 12 on a line | 4.5 | 5.8 | 8.0 |
| 1 pixel: 17 sprites, two runs of each on a line | 4.8 | 5.9 | 8.2 |
| 2 pixels: 8 sprites, two runs of each on a line | 4.4 | 5.5 | 7.7 |
| 2 pixels: sprites of 2x32, 16 on a line | 4.9 | 6.2 | |
| 4 pixels: 4 sprites, two runs of each on a line | 4.1 | 4.9 | 6.4 |
| 4 pixels: sprites of 4x32, 8 on a line | 4.7 | 6.1 | |
| 8 pixels: sprites of 8x32 | 8.5 | 9.3 | |

A run costs **at least about 4.7 clocks untextured, 6 textured, 8 with offset colour** (an eighth of the
triangle's setup each time; 4.1 to 4.5 when the runs are two parts of one polygon, see the modifier volumes); from 8 pixels on it costs its pixels. A second run of the same triangle on the
same line is a little cheaper than the first.

**Many different triangles on a line cost more.** A quad over the tile and K sprites of 1x32 over it, what
each further column costs per line (clocks, with its pixel):

| columns | 1 to 12 | 13, 14 | 15, 16 | 17 to 20 | 21 to 24 | 25 to 28 | 29 to 32 |
|---|---|---|---|---|---|---|---|
| untextured | 4.0 to 4.9 | 5.4 | 5.9 | 6.6 | 8.7 | 11.1 | 10.4 |
| 565 | 4.4 to 6.4 | 6.7 | 7.2 | 9.1 | 10.1 | 12.5 | 12.6 |
| 565, offset colour | 6.7 to 8.5 | 9.3 | 10.9 | 11.5 | 13.2 | 16.1 | 16.5 |

With 32 different triangles on every line a run of one pixel averages 7.1 (sprites of 1x32) to 8.1 (quads
of 1x32) untextured, 8.8 to 10.0 textured, 13.3 with offset colour; with 128 sprites of 1x8 in the tile 9.3. It is
the number of different triangles, not of runs: 17 sprites with 32 runs a line show none of it, and the
same 17 as quads (33 triangles a line) do. Moving the triangles' parameters apart in memory changes nothing.
This is in the opaque pass only: translucent runs of one pixel with 32 triangles a line cost 4.5.

### Sprites of w x h covering the tile, untextured: clocks per sprite

| h \ w | 1 | 2 | 4 | 8 | 16 | 32 |
|---|---|---|---|---|---|---|
| 1 | 39.0 | 39.1 | 39.1 | 39.3 | 39.8 | 40.7 |
| 2 | 39.1 | 39.2 | 39.3 | 39.9 | 40.7 | 69.4 |
| 4 | 39.2 | 39.4 | 39.8 | 40.9 | 70.4 | 139.9 |
| 8 | 74.7 | 44.5 | 44.6 | 69.6 | 139.9 | 277.8 |
| 16 | 120.0 | 84.5 | 79.8 | 139.6 | 276.5 | 558.0 |
| 32 | 226.7 | 157.1 | 150.6 | 272.3 | 554.5 | 1102.0 |

Up to 32 pixels and 4 lines: the setup, 39 to 41. Above 64 pixels in runs of 8 or more: the pixels, about
1.09 each here (1.01 and the tile's 55 to 90). Tall narrow ones: the runs. The tables for quads and for 565
are in tables.txt (section 7); they follow the same rule with 48 per triangle, and a quad is two triangles.

### Translucent and punch through

- A translucent quad over the tile, per layer and pixel (12 layers against 4, the ISP's pass included):
  1.02 untextured, 1.05 with a 565 texture, 1.10 with offset colour, 1.22 bilinear, 1.44 mipmapped
  bilinear, 2.00 palette with ARGB8888 entries, 2.04 YUV bilinear, 3.47 with 8 texels a pixel.
- A small translucent layer costs 52 untextured and 61 textured (1x1), 100 and 114 to 119 (8x8 or 32x1).
- 1024 translucent quads of 1x1: 39.0 per seen triangle, textured or not (the opaque pass: 48 textured).
- Punch through: a 565 quad over the tile costs 1056 more than its twin, where the same quad in the opaque
  list costs 168: its pixels take the TSP's time twice. Per seen triangle 58, per run of one pixel 7.3.

### Modifier volumes: inside against outside

One sheet of a modifier volume lies in front of everything (two triangles, z = 2). Closed as INSIDE_LAST,
the pixels under it are in the volume; closed as OUTSIDE_LAST, none is. The ISP's work is the same in both.
Two pixels of the framebuffer are read back after every scene, to see what the volume did.

**Whether the pixels in a volume are shaded a second time depends on the list.** A quad "with two volumes"
over the tile, first volume 0x406080, second volume 0x202020 blending ONE, ONE:

| list (and modifier volume list) | picture inside | inside - outside, clocks | |
|---|---|---|---|
| opaque (opaque) | 0xffffff | -8 | one pass. The second volume's parameters take the first's place; its ONE, ONE adds to whatever was in the tile's buffer, render after render |
| punch through (opaque) | 0x6080a0 | +1084 | **two passes**: the first volume, then the second on top, 0x406080 + 0x202020 |
| translucent, sorted per pixel (translucent) | 0x202020 with a second volume that blends like the first | -6 | one pass, as in the opaque list. In a pre-sorted tile the volume is ignored (see the end of this section) |

In the punch through list the second volume's ZERO, ONE leaves the first volume's colour, and SRC_ALPHA with
alpha 0x80 leaves it too when untextured and draws the second volume when textured (my reading, not
tested: the second pass has its own alpha test, at PT_ALPHA_REF 0xff, and the texture's alpha is 1). With cheap shadow instead of two volumes
(shadow bit, FPU_SHAD_SCALE 0x180) punch through polygons are shaded twice just the same.

**So for punch through polygons "inside minus outside" is the TSP's work and nothing else**: making the
runs, sorting them, shading. It agrees with the twin method (the same layout in the opaque list, difference
to the twin plus the background's 1035) in every layout:

| layout, in clocks | untextured: inside - outside | twin | 565 Gouraud: inside - outside | twin |
|---|---|---|---|---|
| 1 quad 32x32 | 1093 | 1123 | 1166 | 1209 |
| 4 quads 8x32 | 1268 | 1301 | 1567 | 1638 |
| 32 quads 32x1 | 2514 | 2554 | 3145 | 3168 |
| 64 quads 4x4 | 5023 | 5046 | 6219 | 6257 |
| 32 quads 1x32 | 8244 | 8274 | 10172 | 10209 |
| 1024 quads 1x1 | 39960 | 39982 | 49193 | 49232 |

Two methods that share nothing but the TSP's bit, within 0.1 % where triangles and runs dominate and 3 %
for one quad (the twin method carries the 55 to 90 a tile with seen polygons costs once). Per seen triangle:
**39.0 untextured, 48.0 textured** (565, bilinear, palette, 8 texels a pixel: 48.0 to 48.1). Per pixel, from
a quad over the tile: bilinear 1316 against 1166, palette (ARGB8888 entries) 2148, 8 texels a pixel 3623
(twin: 1376, 2207, 3669).

- **An offset colour of 0 costs nothing** (scenes `vol2off` and `vol2pitch`, `#define TM_VOL2`, log run27;
  tables from `python3 vol2.py <log>`). 565 flat; setup: one tile of 1024 quads of 1x1 against one quad, per
  quad (one seen triangle); pitch: a triangle cut into 1-pixel runs by 15 sheets, as in `spanshad`:

| | setup a quad, offset colour 0 | not 0 | run pitch, offset colour 0 | not 0 |
|---|---|---|---|---|
| no offset bit | 80.1 | 80.1 | 4.21 | 4.21 |
| offset bit, one volume | 80.1 | 96.1 | | |
| offset bit, two volumes (FPU_SHAD_SCALE 0x080) | 80.1 | 96.1 | 4.21 | 6.31 |

  With the offset bit set and the offset colours 0, a polygon costs what it costs without the bit: the
  offset colour's +16 a seen triangle and +2.1 a run are paid only when it is not 0 (0x202020 at every
  vertex, or 0x101010 (k + 1) at vertex k). Two volumes cost the same as one, inside the volume as outside,
  and the offset colour is added in both sets (pixel 0x002028 -> 0x204048 outside, 0x002014 -> 0x204034
  inside). The earlier reading "polygons with two volumes and offset colour cost 48.1 per triangle, where an
  ordinary polygon with offset colour costs 64" (scenes `mv2`) came from their offset colours, which were 0.
  **What counts as 0** (scenes `offs3`, `#define TM_OFFS`, log run28, tables from `python3 offs.py <log>`;
  one tile of 256 quads of 2x2, both triangles of a quad seen, per quad): the test is made per triangle, on
  the offset colours its shading uses, all four bytes. Gouraud: a triangle with any vertex not 0 pays +16
  (0x202020 at v0 only: the quad's first triangle pays, its second not; at v3 only: the second). Flat: only
  the vertex flat shading takes counts (v0 only: nothing; v3 only: +16). Alpha alone (0xff000000, which
  changes no pixel) and blue alone (0x000020) cost as much as all four: the offset colour's four planes are
  taken or dropped together, triangle by triangle.
  **Not shared with mipmapping** (scenes `offs2`, same log): per quad of 1x1 (one seen triangle) 565 Gouraud
  80.10, mipmapped point 80.05, with offset colour 96.06 and 96.02; run pitch 4.21, 4.17, 6.31, 6.26. The
  offset colour adds +16 and +2.1 with or without mipmaps, and mipmaps add nothing.
- **The pass that finds nothing still costs.** Outside minus no volume at all: 370 to 460 for punch through
  polygons, where the sheet's two triangles cost the ISP 42 to 147 in the opaque list. About 300 is the
  second pass with no runs.
- A sheet over part of one punch through quad (inside minus outside, clocks):

| sheet | 1x1 | 2x2 | 4x4 | 8x8 | 16x16 | 32x16 | 32x32 | 32x1 | 1x32 | 2x32 | 4x32 | 8x32 | 16x32 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| untextured | 81 | 94 | 98 | 137 | 338 | 580 | 1093 | 111 | 207 | 215 | 245 | 329 | 600 |
| 565 Gouraud | 146 | 160 | 176 | 212 | 396 | 655 | 1158 | 175 | 296 | 300 | 327 | 434 | 677 |

  A second pass over a block costs **81 + 1.0 a pixel** untextured and 146 + 1.0 a pixel textured (the 81
  and 146 hold the pass's start and the one triangle's setup, 39 and 48). Columns: 32 runs of 1, 2, 4 pixels
  cost 3.9, 4.2, 5.1 each untextured over the 81.

**Opaque list: no second pass.** With the sheet over the whole tile, inside and outside take the same
time, though the picture shows the shadow (half intensity) or the second volume's colours:

| | scenes | inside - outside, clocks |
|---|---|---|
| cheap shadow: 6 formats (untextured, 565, 565 with offset, 565 Gouraud, palette, 8 texels a pixel) in 11 layouts (background only up to 1024 polygons of 1x1) | 54 | -8 to +12 |
| polygons with two volumes: untextured, 565 Gouraud, 565 with offset, 6 layouts | 18 | -11 to +7 |

A single reading is good to 14. The TSP shades the tile once, after the modifier volumes, and whether a
pixel is in a volume is part of what it is shaded as. A volume costs ISP time: 65 for every further volume
of two triangles 32 lines tall (a 32x32 quad in the opaque list: 64), 42 to 147 for the first. A polygon
without the shadow bit: the same time, and no change in the picture.

A sheet over part of the tile does cost TSP time in the opaque list, because **the part of a polygon inside
the volume is a triangle of its own to the TSP**. Inside minus outside, clocks, sheet of w x h at the
tile's corner (each good to about 10):

| scene | 32x1 | 16x32 | 8x32 | 4x32 | 2x32 | 1x32 | 8x8 | 4x4 | 2x2 | 1x1 |
|---|---|---|---|---|---|---|---|---|---|---|
| background only | -2 | -4 | 4 | 3 | 77 | 99 | 5 | 20 | 37 | 42 |
| an untextured sprite over the tile | 4 | -6 | -7 | 0 | 73 | 104 | 2 | 28 | 27 | 28 |
| a 565 sprite over the tile | 6 | -1 | 0 | 31 | 73 | 110 | 6 | 2 | 16 | 14 |
| 1024 untextured sprites of 1x1 | 5 | 1 | 2 | -2 | 2 | -5 | -9 | 1 | 0 | -8 |
| a quad with two volumes | 45 | 16 | 12 | 62 | 89 | 116 | 3 | 14 | 39 | 42 |
| a 565 Gouraud quad with two volumes | 28 | 45 | 44 | 78 | 98 | 115 | 3 | 8 | 21 | 22 |

- One pixel inside costs 30 to 40 untextured: a triangle's setup. 8x8 costs nothing: its 64 pixels take
  longer than the setup. With every pixel a triangle already (1024 sprites of 1x1), a sheet costs nothing.
- Columns give the cost of a run, with its pixels, as (difference + the pixels' time) / 32:

| the part inside is 32 runs of | 1 pixel | 2 pixels | 4 pixels | 8 pixels | 16 pixels |
|---|---|---|---|---|---|
| background only | 4.1 | 4.4 | 4.2 | 8.2 | 16.1 |
| untextured sprite | 4.3 | 4.3 | 4.1 | 7.9 | 16.0 |
| 565 sprite | 4.5 | 4.3 | 5.0 | 8.1 | 16.2 |
| quad with two volumes | 4.6 | 4.8 | 6.0 | 8.5 | 16.7 |
| 565 Gouraud quad with two volumes | 4.6 | 5.1 | 6.5 | 9.5 | 17.6 |

  A run costs at least 4.1 to 4.5 clocks, textured or not, when it is the same polygon's two parts taking
  turns. Runs of different textured triangles cost 5.5 to 6 (the table of runs above): changing the triangle
  is what costs more with a texture. With two volumes the part inside has other parameters, and a run of 4
  pixels costs 6.0 untextured and 6.5 textured.
- A textured spot inside costs about half of an untextured one. I do not know why.

**Translucent list: modifier volumes work when the tile is sorted per pixel, and are ignored when it is
pre-sorted.** (These tests render pre-sorted unless told otherwise, which is why my first try showed
nothing.) A half transparent quad, 0x404040 with no sheet, 0x406080 inside, 0x804020 outside, over the
background 0xc0c0c0; a sheet in the translucent modifier volume list. Pixel read back:

| quad | region array entry | no sheet | inside | outside | sheet behind the quad |
|---|---|---|---|---|---|
| shadow bit, cheap shadow 0x80 | sorted per pixel | 808080 | **707880** | a08070 | 8090a0 |
| | pre-sorted | 808080 | 8090a0 | a08070 | 8090a0 |
| two volumes, second 0x202020 | sorted per pixel | 808080 | **202020** | a08070 | 8090a0 |
| | pre-sorted | 808080 | 8090a0 | a08070 | 8090a0 |
| no shadow bit | either | 808080 | 8090a0 | a08070 | 8090a0 |

- 0x707880 is the quad at half intensity (0x203040) mixed half and half with the background: the volume
  acts on the translucent polygon and on nothing else. An opaque quad under it is not touched (0x8f979f
  against 0x9fafbf: only the translucent part is darker). A sheet behind the quad does nothing.
- Inside takes as long as outside (4137 and 4131 clocks): no second pass, as in the opaque list.
- Bit 0 of ISP_FEED_CFG changes none of this (48 of 48 scenes the same) with six-word region array entries.
- Pre-sorted, the volume is ignored. A sheet behind the quad then makes the tile 510 clocks slower, with the
  same picture.
- refsw2 does the same: it applies the translucent modifier volume list only in a tile that is sorted.

**One thing depends on what was rendered before.** A pre-sorted tile with nothing in its opaque list and a
sheet in the translucent modifier volume list skipped its background: 1000 clocks less, and the quad was
blended with what the tile's buffer held from the render before (0x416080, its own colour at full strength,
instead of 0x8090a0). In the sequence below it stopped after a render of the same quad sorted per pixel,
and the same pre-sorted scene was drawn properly from then on. With an opaque polygon in the tile it did
not happen at all. The sequence, in the order rendered:

| | | picture | TSP's bit, counts |
|---|---|---|---|
| pre-sorted, sheet, over an opaque quad | | 9fafbf (right) | 8263 |
| pre-sorted, sheet, over the background | | **416180** | **6275** |
| sorted per pixel, no sheet | | 808080 | 8247 |
| pre-sorted, sheet, over the background | the scene of the second line | 8090a0 (right) | 8273 |
| sorted per pixel, sheet | | 202020 | 8275 |
| pre-sorted, sheet, over the background | | 8090a0 | 8283 |

It is not simply "until the first sorted render": in another run of the program, sorted renders of other
scenes came long before this sequence and its second line still failed. What sets and clears it I do not
know. The old note in the README (a translucent modifier list "gave a completely black frame on hardware"
under KallistiOS) may be this.

Apart from that, the background is drawn whether or not the tile has an opaque list. With its colour
changed from scene to scene, the picture follows it with pointers to five empty lists, with no list pointer
at all, with no pointer to the opaque list only, and under a half transparent quad without an opaque list,
sorted or pre-sorted (tables.txt, section 15).

## The ISP's end-of-render bit

It is not the end of the ISP's work. With the ISP's work the same (the twin), it comes later when small
triangles are seen:

| scene (untextured) | ISP's bit later by | TSP's bit later by |
|---|---|---|
| 1 sprite 32x32 | 0 | 82 |
| 1 quad 32x32 | 31 | 88 |
| 4 quads 8x32 | 210 | 266 |
| 64 quads 4x4 | 412 | 4016 |
| 32 quads 1x32 | 5104 | 7220 |
| 1024 quads 1x1 | 23756 | 38942 |

And in a render of 16 or more tiles of background it comes 19980 clocks (14.7 tiles) before the TSP's bit,
whatever the number of tiles; with fewer tiles it comes after about 160 a tile. For unseen objects in one
tile it does move with the ISP's work (and with a modifier volume over 1024 sprites of 1x1 it comes 153
later inside than outside, while the TSP's bit does not move), which is why the ISP figures I gave before stand; the TSP figures I
gave before were "TSP's bit minus ISP's bit" and did not.

## How far the ISP runs ahead of the TSP (2026-11-17)

Why the ISP can finish 14.7 background tiles before the TSP: the scenes `ispq` (run 35; `#define TM_ISPQ` runs
only them, about 10 s). The first n tiles of the screen (8, 16, 32, 64), each with the content below; the ISP's
bit against the TSP's at 64 tiles, the TSP's pace from 32 to 64 tiles, and the runs still to be shaded when the
ISP ends (the lead less the last tile's write-out, times the runs of a tile):

| a tile | primitives | runs | lead, clocks | lead, tiles | TSP's pace a tile | runs left |
|---|---|---|---|---|---|---|
| background, 0888 framebuffer | 0 | 32 | 19986 | 14.6 | 1366 | 436 |
| background, 565 framebuffer | 0 | 32 | 15250 | 14.6 | 1041 | 437 |
| one sprite over the tile | 1 | 32 | 19916 | 14.6 | 1366 | 435 |
| 2 sprites of 32x16 (rows) | 2 | 32 | 19894 | 14.6 | 1366 | 434 |
| 4 sprites of 32x8 (rows) | 4 | 32 | 19874 | 14.5 | 1366 | 434 |
| 2 sprites of 16x32 (columns) | 2 | 64 | 10800 | 7.9 | 1365 | 443 |
| 4 sprites of 8x32 (columns) | 4 | 128 | 6288 | 4.6 | 1366 | 462 |
| 32 sprites of 1x32 (columns) | 32 | 1024 | 4707 | 0.7 | 7205 | 476 |
| 32 sprites of 32x1 (rows) | 32 | 32 | 2882 | 1.7 | 1736 | (28) |

- **What is queued between ISP and TSP is runs**: about 430 to 480 of them are still to be shaded when the ISP
  ends, whatever the tile holds, which fits a queue of about 512 runs. A background tile is 32 runs (one a line),
  hence the 14.6 tiles.
- Not time: with a 565 framebuffer the TSP takes 1041 a tile instead of 1366, and the lead is the same 14.6 tiles.
- Not tiles: four columns of 8x32 lead by 4.6 tiles.
- Not primitives: four rows of 32x8 lead by 14.5 tiles where four columns of 8x32 (same primitives, four times
  the runs) lead by 4.6.
- 32 rows of 32x1 a tile are something else: in many tiles they take 1736 a tile, more than the ISP (32 sprites,
  about 19 each) or the TSP (40.7 a sprite in one tile) should; the ISP does not get ahead there, and the 28 runs
  left say nothing about the queue. Not explained.
- Not known: whether a run that a nearer polygon cuts in two counts twice (here every run is a whole primitive's
  line), and what an entry holds.

## What limits 32 rows of 32x1 a tile (2026-11-17)

The `rows` scenes (run 36; `#define TM_ROWS`, about 10 s). 32 sprites of 32x1 a tile take 1736 a tile (54 a row)
in a render of many tiles, more than the ISP (~19 a sprite) or the TSP (40.7 a sprite in one tile) needs.
Primitives that never pass the depth test (1x1 sprites, compare NEVER) are ISP work only: they make no runs
and get no TSP setup. Pace a tile from 32 to 64 tiles:

| a tile | pace | a row | against plain rows |
|---|---|---|---|
| 32 rows untextured | 1736 | 54.3 | |
| the same + 16 / 32 / 64 primitives that never pass | 1989 / 2246 / 2903 | | +15.8 / +15.9 / +18.2 each |
| 32 rows 565 | 1951 | 61.0 | +6.7 a row (TSP setup +9) |
| 32 rows 565 with offset | 2047 | 64.0 | +9.7 a row (TSP setup +25) |
| 32 rows untextured, DCalcCtrl 1 | 1699 | 53.1 | -1.2 a row (TSP setup +5) |
| 32 rows as quads (two triangles) | 2496 | 78.0 | = 2 x 39 |
| control: 4 rows of 32x8 + 0 / 32 / 64 that never pass | 1366 / 1366 / 1452 | | the ISP alone is the limit at +64 |

- **Not a stage after the ISP that counts runs**: primitives with no runs add their ISP cost (~16) to every
  tile, while the ISP's own work stays far below the pace (32 x 19 + 32 x 19 + ~160 = ~1376 with 32 added).
- **Not one setup unit shared by ISP and TSP**: DCalcCtrl's 5 clocks of TSP setup add nothing, and offset rows
  take the TSP's offset setup (64) alone, not 64 + 15.
- **A per-primitive resource both use, whose cost grows with the parameters' size** fits: untextured rows ~16
  (the ISP's part) + ~38, 565 +7, offset +10, and in textured rows the ISP's own bit slows too (1843 and 1929 a
  tile against 1737), although the ISP does not look at textures. **[G]** One parameter fetch (VRAM reads of each
  primitive's parameters) for the ISP and the TSP. *(Rejected by run 37, next section.)* Quads are bound by the TSP's setup of two triangles (2 x 39).

## Is it a parameter fetch shared by the ISP and the TSP? No (2026-11-17)

The `pfetch` scenes (run 37; `#define TM_PFETCH`, about 10 s) test the guess above. If one fetch of each
primitive's parameters served both units, a bigger primitive or more VRAM traffic beside it would slow the
rows. An offset colour of 0 adds 4 words to each sprite (27 against 23) but no TSP setup. Pace a tile from 32
to 64 tiles; lead: the TSP's end less the ISP's at 64 tiles:

| a tile | words a sprite | pace | a row | ISP's bit | lead |
|---|---|---|---|---|---|
| 32 rows untextured | 19 | 1736.6 | 54.3 | 1736.6 | 2868 |
| the same, 565 framebuffer | 19 | 1737.8 | 54.3 | 1737.8 | 2528 |
| the same, no write-out (all tiles but the last) | 19 | 1735.9 | 54.2 | 1735.9 | 2840 |
| 32 rows 565 | 23 | 1952.8 | 61.0 | 1824.7 | 22266 |
| 32 rows 565, offset colour 0 | 27 | 1962.2 | 61.3 | 1971.7 | 13586 |
| 32 rows 565, offset colour | 27 | 2047.9 | 64.0 | 1931.4 | 27170 |
| 32 rows untextured + 32 never passing, 19 words | | 2249.4 | | 2249.0 | 2868 |
| the same, the never passing ones 27 words (565 offset 0) | | 2248.9 | | 2248.9 | 2868 |

- **Size costs nothing.** 4 more words a passing sprite: +0.3 a row (2 beats of VRAM would be +2). 8 more
  words a never-passing sprite: 16.01 against 16.03 each. The offset rows' +10 a row of run 36 is the offset
  colour's setup (+2.7 a row here over offset 0), not their 4 words.
- **VRAM traffic costs nothing.** Half the write-out (565 framebuffer) or none: +1.3, -0.6 a tile.
- **The ISP keeps pace in untextured rows.** Its bit is the pace, and it ends ~2870 clocks before the TSP
  (one tile's TSP work and write-out) at 32 and at 64 tiles, with or without the never-passing extras. In
  ispq it led by ~15 tiles. In textured rows it leads by 7 to 13 tiles and its bit (1825 to 1972) is not the
  same from one scene to the next, so "the ISP slows in textured rows too" (run 36) is not evidence.

So the cost is not a fetch of the parameters. Left: the ISP is the limit in untextured rows (54 a passing
32x1 sprite, 16 a never-passing 1x1), or something other than the run queue holds it to the TSP. Not known.

## What changed against my earlier numbers

- "The TSP cannot start before 13.5 µs": that was the background of KallistiOS's first region array entry.
- "A triangle 1 pixel wide costs 15.9, wider ones 39.6": wrong, every seen triangle costs 39 (48, 64).
- "Per span only a floor of 3.1": 4.5 to 5, and more on crowded lines.
- Per pixel 1.09 / 1.17 / 1.34 / 2.15 / 3.57 (untextured / point / bilinear / palette / 8 texels): they
  contained fixed parts and the first tile's texels. 1.01 / 1.01 / 1.11 / 2.00 / 3.26.
- "Every further tile takes 1358 at least": only with a 32 bit framebuffer. 1042 with 16 bits.
- "A palette costs one more clock a pixel": only with ARGB8888 palette entries, which is what the test
  had. With 16 bit entries it costs nothing.
- "Pointers to the two modifier volume lists add nothing" (first version of this report): the tile had no
  such lists then. They add 30 and 20.
- "The pixels in a volume are not shaded a second time" (second version): true in the opaque list. In the
  punch through list they are shaded twice, as you said.
- "A translucent modifier volume does nothing" (third version): the tile was pre-sorted. Sorted per pixel
  it works.

## DCalcCtrl (2026-11-17)

The bit of the ISP/TSP instruction word that says where the mipmap level's numerators are taken: 0 at the
screen's origin, 1 at the polygon's first vertex (what it does to the level: README, mipmeas `mpos1`). Scenes
`dcalc` / `dcalcH` (run with `#define TM_DCALC`, a few seconds; log `fit/timemeas_logs/timemeas_run10_dcalc.log`,
table 20): five formats in five layouts with the bit 0 and 1, each against its twin, two passes.

- **DCalcCtrl 1 costs the TSP 5 clocks per seen triangle**: 5.00 +- 0.02 with 512 and 1024 triangles, in
  every format, untextured and without mipmaps as well as mipmapped point, bilinear and trilinear. The bit is
  not looked at in the light of the texture: the setup does the extra work for any polygon that has it.
- **It is setup time.** A triangle that its pixels keep busy longer hides it (8x8 quads bilinear or trilinear:
  0; untextured and point sampled 8x8 quads are still bound by their setup: 4.7 to 4.8), and nothing
  is added per pixel (one 32x32 quad: within the noise of +-11 clocks, under 0.01 a pixel).
- **The ISP does not see it**: the twins, the same triangles behind the background, take the same time with
  either bit (within +-8 clocks, no trend).
- In the terms of "Per triangle" above: the setup of a seen triangle goes from about 39 to 44 untextured and
  from 48 to 53 textured with the bit set.

**How it is hidden.** The TSP sets up a polygon while it shades the runs of polygons before it, so extra
setup is free where the run stage has more to do than the setup stage. But it is not simply the larger of
the two totals. Sprites covering the tile (scenes `dcalcs`, table 21; clocks per sprite, untextured / 565):

| sprite | DCalc 0 | DCalc 1 adds | | sprite | DCalc 0 | DCalc 1 adds |
|---|---|---|---|---|---|---|
| 1x1 | 39.0 / 48.1 | 5.0 / 5.0 | | 2x8 | 44.5 / 55.5 | 4.8 / 4.7 |
| 2x2 | 39.1 / 48.3 | 5.0 / 5.0 | | 4x8 | 44.7 / 56.6 | 1.5 / 0.7 |
| 4x4, 8x2, 16x1 | 39.7-39.9 / 49.4-49.7 | 4.8-4.9 / 5.0-5.1 | | 8x8 | 69.6 / 76.4 | -0.5 / 0.0 |
| 8x4, 16x2, 32x1 | 40.4-41.0 / 51.2-51.8 | 4.9-5.7 / 4.6-5.4 | | 32x2 | 70.4 / 77.4 | -0.4 / 0.8 |
| | | | | 1x8 | 74.6 / 92.0 | 9.5 / 9.6 |

- Up to 4 lines a sprite is bound by its setup (the 0.5 to 2 above it are the tile's fixed time spread over
  the sprites) and pays the 5 in full.
- Sprites of 8 lines that take longer than the setup still pay in full (2x8: 44.5 + 4.8) or in part (4x8:
  44.7 + 1.5), where "the larger of the two" would make them pay nothing. What counts is where the work
  falls in the tile, not its total: the TSP goes through the tile line by line, and every sprite of a row
  of sprites needs its setup at the same line, the first of the row (16 of them for 2x8, 8 for 4x8). My
  reading, not measured directly: the setup can only work ahead so far, so such a burst stalls the runs, and
  only the runs of the other lines give it time to hide in.
- Long runs (8x8, 32x2: 8 pixels or more a run, 64 pixels a sprite) hide it completely.
- 1x8 sprites pay about twice the 5: 32 different sprites on every line, more than the TSP keeps (see
  "Many different triangles on a line" above), and a sprite is set up about twice on average. So that
  crowding cost is, at least in part, setting triangles up again.

**Latency: never seen.** A primitive's first pixel needs its mipmap constant, so the question is whether the
pixels wait for the whole setup or start on the planes they need while the rest is worked out. Scenes
`dcalct` (table 22): the first 64 tiles, each covered by one primitive of its own (a sprite, or a quad of
two triangles), 16 bit framebuffer so that the TSP and not the write-out sets the pace:

| one primitive a tile | per tile, DCalc 0 / 1 (clocks) |
|---|---|
| untextured sprite / quad | 1041.0 / 1041.3, 1043.0 / 1043.0 |
| 565 sprite / quad | 1444.6 / 1444.9, 1451.5 / 1449.9 |
| 565 mipmapped point, sprite / quad | 1362.6 / 1364.0, 1423.1 / 1424.9 |
| 565 mipmapped bilinear, sprite / quad | 1730.9 / 1729.4, 1777.1 / 1778.4 |

(the passes differ by up to 1.5 a tile). A tile does not pay the 5, and does not even pay the setup
itself: an untextured tile is its 1024 pixels at 1.01 and 7 clocks, not 1035 + 39. So the setup of a tile's
first primitive is done while the tile before is still being shaded or written out, and in a tile alone
(3586 either way) it is hidden behind the tile's start. The setup runs ahead of the pixels across
primitives and across tiles; it shows only where it cannot keep ahead: many small primitives, or many
first runs on one line. Whether a primitive's pixels could start before its setup is complete can therefore
not be seen in the timing; what is seen is that the setup is the same sequence for every primitive (the 5
clocks are paid by untextured ones, which have no level to compute).

**How far ahead: about 11 primitives.** Scenes `ahead` (one tile) and `ahead2` (the same in 64 tiles, 16 bit
framebuffer; table 23; run with `#define TM_AHEAD`): a block keeps the pixels busy, then N sprites of one
pixel on the next line need their setups at once, with DCalcCtrl 1 on those sprites only. The time the bit
adds, divided by 5, is the number of sprites whose setup was not done ahead; N minus that is how many were:

| N | 16 | 18 | 20 | 22 | 24 | 26 | 28 | 30 | 32 | mean from 16 |
|---|---|---|---|---|---|---|---|---|---|---|
| block 565, texels 8 apart, lines 16-23 (about 840 clocks) | 11.2 | 10.7 | 10.9 | 9.2 | 11.4 | 11.3 | 11.2 | 11.2 | 11.2 | 11.2 |
| the same over lines 20-23 (about 420) | 9.6 | 10.0 | 11.1 | 9.5 | 11.7 | 11.8 | 11.5 | 11.3 | 10.4 | 11.1 |

Below about 12 nothing is paid. The plain times say the same: the first 12 or 13 sprites of the burst cost a
few clocks each, every one after that about 40 (a setup on the critical path). Twice the time to work
ahead gives the same number, so it is a limit of storage, not of time: the TSP holds about 11 or 12 set-up
primitives ahead of the one it shades (counting only new ones; whether the block and the background,
which are still in use or about to be, take places of their own is not told). It is the same number as
the "more than about 12 different triangles on a line" of the runs above, and the 1x8 sprites being set up
about twice: one store of set-up primitives, about 12 deep, for both. One tile alone (`ahead`) gives 8 to 11
with more noise. Not understood: with the block over lines 0-23 (2500 clocks) the plain times jump up
and down by up to 200 a tile from one N to the next and the count wanders between 10 and 23.

That fits the level's model: per pixel the level is a polygon's constant times w^2, and the constant is made
in the setup. With DCalcCtrl 1 the setup also evaluates the planes at the first vertex (a x1 + b y1 + c);
5 clocks for that is what a pass of the 1/w, U and V planes through a shared multiplier would take (a guess).

## Run starts are made ahead of the shading too (2026-11-17)

Scenes `spanahead`, `spanburst`, `spanpos`, `spanq` (run with `#define TM_SPAN`; logs timemeas_run15..19).
In each of 64 tiles one 565 triangle covers the tile (one primitive: no setups), with the shadow bit and
cheap shadow; opaque modifier volumes of one pixel's width cut chosen lines into runs of one pixel (a run
ends where the volume state changes: README, uvrun). 16 bit framebuffer; a tile takes about 1473 clocks
(1.44 a pixel), which the TSP sets.

- **A run start has a cost of its own, also within one triangle.** With every line of the tile cut into 31
  runs, each run of one pixel costs **4.1 to 4.2 clocks**, and offset colour adds about 2 more to each (the
  6 and 8 of "Per run" above): it is the work of starting a run, not of changing triangles.
- **A burst of them is hidden.** One line of 30 runs costs 15 to 47 clocks a tile; from about four such
  lines on every further line costs its full 30 x 4.2. What is hidden is about **45 to 70 runs, 180 to 290
  clocks** a tile:

| cut lines start at | line 1 | line 8 | line 20 |
|---|---|---|---|
| runs hidden (L = 4 .. 8 lines cut) | 43 | 49 to 52 | 50 to 52 |

| runs on the lines before the cut ones | 32 pixels | 16 pixels | 8 pixels |
|---|---|---|---|
| runs hidden | 52 | 68 | 66 |

  (two passes of every scene agree within 2 clocks a tile.)
- It does not depend on where in the tile the cut lines are, so the run starts are made ahead across the
  tile boundary, during the tile before. And it does not shrink when the runs queued before the cut lines
  are 4 times shorter, so what is queued is not a number of runs but an amount of shading: about 150 to 200
  pixels (at 1.4 clocks) of runs started ahead of the pixels being shaded. My reading: a queue of pixels (or
  of their run's values) between the stage that starts runs and the stage that shades, about 5 or 6 lines
  of a tile deep.
- So the TSP has two stages that run ahead of its shading: the setup of primitives (about 11 primitives
  ahead, table 23) and the start of runs (about 150 to 200 pixels ahead). In the 4.7 / 6 / 8 a run of the
  table "Per run" the queue was always empty: every line had the same short runs.
- **By format** (scenes `spanfmt`, `#define TM_SPANFMT`, log timemeas_run20_spanfmt, table 25): the same 15
  columns over lines 0 .. L - 1, L = 0, 8, 16, 24, 32; clocks a run start, by the 8 lines added (two passes
  agree to the clock):

| | lines 0-7 | 8-15 | 16-23 | 24-31 (nothing left to hide in) |
|---|---|---|---|---|
| untextured, flat or Gouraud | 3.03 | 3.08 | 3.09 | **3.94** |
| 565, flat or Gouraud | 3.37 | 3.29 | 4.13 | **4.21** |
| 565 with offset colour, flat or Gouraud | 5.16 | 5.68 | 6.21 | **6.31** |

  The cost per run grows as fewer uncut lines are left after the cut ones: the queue still hides part of
  the run starts while long runs follow. The last 8 lines show it whole: **3.9 untextured, 4.2 textured, 6.3
  with offset colour.** Flat and Gouraud are the same to the clock in every case, so the colour channels are
  not what a run start costs (they are evaluated, or not, alike); the texture's two planes add 0.3, the
  offset colour 2.1. It is not so many clocks per plane.
- **These are pitches, not latencies**: the slope of the time over more and more runs in a stream, where each
  run adds it. A latency would be paid once per stall, and the queue (about 150 to 200 pixels of shading)
  hides any latency shorter than that; none was seen.
- **What the pitch depends on** (scenes `spanshad`, same cuts, 565 triangle; lines 24-31, two passes agree
  to the clock): modulate 4.21, decal 4.21, modulate alpha 4.21 (the planes a shading instruction does not
  use are not skipped); offset colour with modulate 6.31, with decal 6.30; a polygon with two volumes 4.21,
  with offset colour 6.31 (`vol2pitch`, run27), as with one volume. (Run21's two-volume scenes, `spanshad` m
  5 and 6, had FPU_SHAD_SCALE 0x180: in intensity mode such polygons were drawn in their base colour only,
  no texture and no offset colour, at 3.26 a run. `spanshad` now sets 0x080 for them.)

| run start | pitch, clocks |
|---|---|
| runs of different triangles (table "Per run" above): untextured / 565 / 565 offset | 4.7 / 6 / 8 |
| one triangle cut by cheap-shadow volumes: untextured / 565 / 565 offset | 3.9 / 4.2 / 6.3 |
| one two-volume triangle cut by volumes: 565 / 565 offset | 4.21 / 6.31 |
| 565 with the offset bit and an offset colour of 0, one or two volumes | 4.21 |

  So the pitch is not so many products over so many multipliers: flat and Gouraud, decal and modulate cost
  the same, the texture's two planes add 0.3 in one context and 1.3 in another. The one increment that looks
  like arithmetic is the offset colour's 4 planes: +2.1 (cheap shadow) and +2 (different triangles), about
  2 planes a clock; if a plane's run start is A x0 + B y + C, that is 4 products a clock (or 2, if B y + C
  is kept for the line), and only for an offset colour that is not 0. The rest, 3.9 to 4.7 clocks a run,
  follows the context: whether the run is another triangle's (its coefficients loaded again) or the same
  triangle's after a volume boundary (cheap shadow, or the other set of a two-volume polygon: the same
  4.21). My reading, not measured.

- **Is part of a run start kept for the line?** (scenes `lineshare` to `lineshare5`, `#define TM_LINE`, logs run22
  to run26, tables from `python3 lineshare.py <log>`). Lines of one-pixel runs; the cost of such a line is the
  slope over the number L of them (per-tile costs drop out). The same scene in different runs agrees to 0.1 a
  line; the two passes of a run to 0.1 to 3 clocks a tile.
  - `lineshare` (run22, untextured): a triangle whose runs repeat on the line costs 2.0 a line less than the
    same runs from different triangles, at column 0 and 16 alike; runs further right cost nothing more.
  - `lineshare2` (run23, 24): R is a sprite T behind K sprites of 1 x L at columns 1, 3, .. (K + 1 first and K
    repeated runs a line), D the same runs from 2K + 1 different sprites, with a sprite behind everything so
    that the plain lines are the same. R - D a line:

| format | K 3 | K 4 |
|---|---|---|
| untextured | -2.2 | -1.9 |
| 565 | -10.4 | -12.0 |
| 565 with offset colour | -9.9 | -14.1 |

  - `lineshare5` (run26; `lineshare3` and `lineshare4`, runs 24 and 25, are parts of it): 565 with offset
    colour, a pattern line is a first run, M sprites of 1 x L and a tail: R = T, S1..SM, T; R' = T, S1..SM, U;
    D = U0, S1..SM, U. A pattern line over a plain one, first run 1 pixel (2 pixels gives the same within 1.3):

| M | R | R' | D | R' - R: the tail is T's second run | D - R': the first run is T or U0 |
|---|---|---|---|---|---|
| 1 | -0.2 | 2.5 | 3.4 | 2.7 | 0.9 |
| 2 | 5.0 | 11.3 | 12.6 | 6.3 | 1.3 |
| 3 | 14.5 | 21.4 | 21.0 | 6.9 | -0.5 |
| 4 | 24.8 | 30.4 | 31.0 | 5.6 | 0.6 |
| 5 | 34.9 | 36.4 | 36.3 | 1.6 | -0.1 |
| 6 | 40.8 | 45.0 | 47.8 | 4.2 | 2.8 |
| 7 | 52.5 | 53.9 | 52.4 | 1.4 | -1.5 |
| 8 | 55.5 | 57.8 | 55.7 | 2.3 | -2.1 |

  A triangle's second run on a line costs less than another triangle's run: 6 to 7 clocks less with 2 or 3
  runs between, 5.6 and 4.2 with 4 and 6, 1.4 to 2.3 with 5, 7 and 8. A first run of the triangle of the
  lines around it costs the same as any other first run. A run added to a line costs 3 to 12 clocks,
  alternating with the number of runs on the line (R: 5.2, 9.6, 10.3, 10.0, 5.9, 11.7, 3.0 from M 1 to 8; R'
  and D alike), the same at both parities of column: it follows the count of runs, not their columns.
  So this is not a fixed part of every run start (C + B y) kept for a line and triangle: what a repeated run
  saves, and what a run costs, depend on how many runs the line has. A stage that takes a line's runs
  together (sorting or grouping them over a window of a few runs) would fit better; my reading, not shown.
  The rounding of the values (README, uvspan: each product cut by itself, screen coordinates) allows either.
- **Slow texturing before a burst: nothing shown, and the write-out is not the unevenness** (scenes `spantex`,
  `spantex2`, `spantex3`, `#define TM_SPANTEX` / `TM_SPANTEX3`, logs run31 to run34, tables from `python3
  spantex.py <log>` and `python3 spantex3.py <log>`). 64 tiles, each a 565 triangle T, a sprite U over lines
  0 .. K - 1 (565, YUV422 bilinear, or 8 texels a pixel), and a burst of 30 L one-pixel runs (15 sheets) on
  lines 20 .. 20 + L - 1. spantex3 repeats it with the tiles written out at 16 bits, not written out (all but
  the last), and written out at 0888. Clocks a tile, all lines 565, steps for two more cut lines:

| | L 0 -> 2 | 2 -> 4 | 4 -> 6 | 6 -> 8 | 8 -> 10 | 10 -> 12 | tile, L 0 |
|---|---|---|---|---|---|---|---|
| 16 bit, written out | 99 | 194 | 125 | 376 | 280 | 65 | 1474 |
| 16 bit, not written out | 453 | 209 | 123 | 379 | 279 | 65 | 1104 |
| 0888, written out | 73 | 98 | 98 | 86 | 276 | 68 | 1918 |

  - From L = 2 on the steps are the same with and without the write-out: the uneven cost of a cut line by
    where it is in the tile (lines 26-27 about 188 each, 30-31 about 33) is not the write-out's. At 0888 the
    write-out sets the pace (1918 a tile) and hides most of it. Not understood.
  - The step from L = 0 is where the 15 sheets (30 modifier volume triangles a tile) first appear; it holds
    their fixed cost when it shows (453 without the write-out, where the tile is short).
  - What spantex and spantex2 had shown (a burst after slow lines costs more; 4 slow lines at the top take
    about 300 clocks away that 16 fast lines do not give back) is all in that first step: from L = 2 on, the
    rows with slow lines add the same as the 565 rows (16 bit written out, 8 texels on lines 0-3: within
    about 60 of the 565 row at every L). So it tells nothing about where the queue lies; the reading "the
    queue is after the texturing", written here before, is withdrawn. Whether texturing is before or after
    the queue is not known.
  - The write-out costs about 370 a tile here even at 16 bits (1474 written out, 1104 not), though it is
    shorter than a tile: a tile's write-out and the next tile's shading do not fully overlap.
  - The old spanpos numbers (log run19), steps per added cut line from L = 1 on, also vary from 10 to 187:
    the "about 45 to 70 runs hidden" above sums uneven steps and is to be taken as rough.

- **Do setup and run starts share the arithmetic?** (scenes `share`, `#define TM_SHARE`, logs run29 and run30,
  tables from `python3 share.py <log>`). 64 tiles, a triangle T over each, lines 0 .. L - 1 of it cut into 31
  one-pixel runs by 15 cheap-shadow sheets (paced by run starts); at column 31 of those lines a 16th sheet (32
  runs, all of T), or a sprite of 1x1 on every line (one more triangle, so one more setup, a line), or one
  sprite of 1 x L (the same runs and triangle changes, one setup a tile). The same without the 15 sheets gives
  lines paced by shading. Clocks a line (run30; run29 the same within 0.2):

| | 565 | 565 with offset colour |
|---|---|---|
| a 16th cut (one more run of T) | 4.1 | 4.2 |
| the tall sprite instead: two triangle changes a line | +1.7 | +2.2 |
| a 1x1 sprite on every line: a setup a line on top, paced by run starts | **+13.7** | **+22.8** |
| the same, paced by shading | +30.6 | +39.7 |
| a triangle's setup where setups set the pace (table "Per triangle") | 48 | 64 |

  Changing triangle costs about 1 clock a change. A setup takes 13.7 (565, 7 planes) and 22.8 (offset
  colour, 11 planes) from a line paced by run starts: about 2 clocks a plane (the offset colour's 4 planes
  9.1, 2.3 each); the rest of the setup, 34 and 41, is made alongside. So the setup's work per plane and
  the run starts go through one unit, and the rest of the setup does not. My reading of the rates: one unit
  of about 4 products a clock fits them all, with a plane's setup about 9 products (A, B and C each a sum
  of three: 2.25 clocks) and a run start 2 products a plane (A x and B y: 0.5 a plane, the offset colour's
  +2.1 for 4 planes, hidden under the run's about 4 clocks up to 7 planes). With C + B y kept for the line a
  run start would be one product a plane, 2 a clock against the setup's 4: possible, but not what the rates
  suggest.
- Not understood: the cut lines 20 to 25 (L = 6) give 80 to 87 hidden in two series and 50 in the third,
  where L = 4 and 8 agree. The texels-8-apart triangle (spanahead v 0) gives erratic times, as the block of
  the "ahead" scenes did.

## In what order the TSP shades a tile: the span sorter (2026-11-17)

Scenes `order` (runs 38 and 39; `#define TM_ORDER`, a few seconds; `python3 pvr-tests/timemeas/order.py`). Timing
the end of a render cannot tell in what order a tile's runs are shaded, so these scenes read it from the picture.
**The palette clock:** every pixel of the scene, background included, is an 8 bpp palette texel of index 0,
decal, point sampled, and while the render runs the CPU rewrites palette entry 0 (ARGB8888) with a counter, one
write every ~25 counts (12 clocks). A pixel's colour is the counter when the TSP looked the entry up: when that
pixel was shaded, to about 5 pixels (ARGB8888 entries cost 2 clocks a pixel). About one pixel in a scene reads a
value 116 to 120 writes old (probably a write meeting the read); order.py sets these glitch pixels apart. One
tile, opaque list, sprites at one depth with compare ALWAYS (the one sent last is seen).

- **A primitive's runs are shaded together.** A sprite T over the tile with columns S1 and S2 in front (a line
  is T, S1, T, S2, T): T over the whole tile in raster order, then S1's column, then S2's.
- **Primitives go in the order they first appear in the tile (raster order), not in list order.** 32 columns
  sent left to right or right to left, 16 sprites of 8x8 and 1024 of 1x1 sent in a shuffled order: the same
  order on the console.
- **Many primitives on every line: the tile is swept in batches.** 32 columns of 1x32: column 0 lines 0-3,
  column 1 lines 0-3, ..., column 10 lines 0-4, ..., column 31 lines 0-7, then column 0 again from line 4. Each
  visit sets the column up again (about 3.5 writes, ~45 clocks, a column a sweep, the textured setup's 48).
- **A primitive being shaded takes its runs that come in meanwhile.** T at (0, 0) and on lines 3-31, 95 sprites
  of 1x1 on the rest of lines 0-2: T's 30 lines first, then the 95 in raster order.

**The model** (order.py `sort_runs`): the tile's runs (a run: the longest stretch of one primitive on one line)
enter a window of **96 runs** in raster order as soon as there is room. Repeatedly the oldest run in the window
chooses the primitive; it and every run of that primitive in the window are shaded, oldest first, including the
runs that enter while it lasts (each one shaded makes room for the next). Runs out of order along the model's
order, all 23 scenes, by window size (`order.py --fit`):

| window | 92 | 94 | 95 | **96** | 97 | 98 | 100 |
|---|---|---|---|---|---|---|---|
| run 38 (scenes 0-17, 9,800 runs) | 395 | 235 | 129 | **6** | 150 | 247 | 385 |
| run 39 (scenes 0-22, 17,000 runs) | 861 | 495 | 246 | **20** | 276 | 480 | 704 |

At 96: 13 of the 26 are glitch pixels, the other 13 are the early drains below. What the scenes tell apart
(scene numbers as in main.c):

- **The window counts runs, not pixels:** a window of 96 pixels puts 4 or 5 runs out of order in 4 columns of
  8x32 and in 16 sprites of 8x8, 10 to 48 in scenes 13 to 15. A run of 16 or 24 pixels takes one place: one
  place per 16 pixels puts 10 and 37 out of order (scenes 14, 15), one per 8 pixels 10 to 72 (13 to 15).
- **A primitive comes back at the end:** once drained, it waits behind every primitive whose runs came in
  before its next one (that is the oldest-run rule). Keeping each primitive's place in a fixed sweep instead
  puts 11 and 97 out of order (scenes 15, 16, run 38).
- **No limit on different primitives below 96:** limits of 64 or 32 put 35 and 15 out of order (scene 16, 64
  primitives with up to 64 in the window).
- **Freed places are used again at once:** a circular FIFO whose new runs must lie within so many places of the
  oldest run still waiting (holes left by drained runs wait for the oldest to pass) puts 485 to 631 runs out of
  order at 96 to 320 places (run 38).
- **Each tile on its own:** two tiles of 32 columns are sorted alike, the second after the first's last pixel;
  also with primitives over both tiles (run 41, next section).
- **The background polygon is a primitive like the others** (scene 18), apart from the early drains.

**Not explained: early drains.** Some whole drains are shaded before the 2 to 9 drains in front of them,
holding the runs the model gives them (its choice of runs is right; the time order of whole drains after it is
not). Column 31 in 32 columns of 1x16 (scenes 11, 20, 21, twice each, always the same drains), where the same
columns 1x32 (scenes 3, 22) are in order through the same sequence of drains; and in 16 columns over the
background (scene 18) the background and column 0 change places when one follows the other, and column 0 goes
about 2 drains early.

**[I] Hardware:** a store of 96 runs (parameter address, line, x, length: about 40 bits each, ~480 bytes) between
the ISP's run queue and the TSP's setup, with the head entry's primitive compared against every entry; matching
entries leave in age order and the queue refills free entries at once. The TSP's setup works on what leaves it:
a primitive is set up once per drain, so a tile with many primitives on a line sets each up once per sweep.
Whether that is the "more than about 12 different triangles on a line" cost (a sweep visiting more primitives
than the TSP keeps set up) is not checked: 17 nested sprites with two runs each a line showed no such cost.

## How fast the ISP's side cuts a tile into runs (2026-11-17)

Scenes `cut` and `cutn` (run 40; `#define TM_CUT`, a few seconds; analyze.py section 30). Twins: the same
untextured sprites 1 x h at every other column, passing (compare ALWAYS: they and the background between them
are runs) or never passing (NEVER: only the background's 32 runs). The ISP's work is the same either way (section
"ISP"), so the difference of the ISP's bits is what the extra runs cost on its side. The runs are made before the
queue between ISP and TSP (it counts runs), so this is where they are cut.

| one tile | more runs | lines they are in | ISP's bit, more (clocks) |
|---|---|---|---|
| 1 / 2 / 4 / 6 sprites 1x32 | 64 / 128 / 256 / 384 | 32 | 61 / 126 / 252 / 383 |
| 4 sprites 1x16 | 128 | 16 | 127 |
| 8 sprites 1x8 | 128 | 8 | 126 |
| 16 sprites 1x4 (a line of 32 runs of 1 pixel) | 124 | 4 | 126 |
| 16 sprites 1x8 | 248 | 8 | 252 |

- **One clock a run, whatever its length.** 0.95 to 1.02 a run in every placement; the same runs spread over 32
  lines or packed into 4 cost the same. A line of 32 one-pixel runs costs 31 more than a line of one 32-pixel run,
  so a whole line is one clock when it is one run. Cutting 8 or 4 pixels a clock would give 112 or 96 instead of
  126 for the 1x4 sprites. **[I]** The tag line's boundaries are found all at once (31 compares of neighbours) and
  the runs leave one a clock, in raster order.
- **The ISP's bit waits for the last tile's cutting**: with the sprites in the last of two tiles the bit moves by
  130 and 254 for 128 and 256 more runs, whatever the other tile holds.
- **The next tile's work hides part of it.** With the sprites in the first tile, the ISP's bit, ALWAYS less NEVER,
  with W sprites 1x1 that never pass in each following tile (ISP work only, about 19 each):

| following tiles | W 0 | W 8 | W 16 | W 32 |
|---|---|---|---|---|
| 128 more runs, one tile after | 70 | 19 | 35 | -2 |
| 128 more runs, two tiles after | 70 | 1 | 0 | 6 |
| 256 more runs, one tile after | 245 | 182 | 175 | 2 |
| 256 more runs, two tiles after | 238 | 89 | -5 | 0 |

  So the ISP does not stop while a tile is cut: enough work after it (32 sprites in the next tile, or 16 in
  each of two) hides the whole cutting. It is not a pair of tag buffers that lets the next tile go on freely
  either: then a tile with nothing but the background after it would hide the same amount behind 128 and 256
  more runs, and it hides 58 and 11. How the work overlaps is not known.
- Two tiles with primitives over both (`order` scenes 23 and 24, run 41): each tile is sorted on its own and
  the second starts after the first's last pixel; a sprite over both tiles drained in the first does not take
  its runs in the second.

## Not known

- What makes runs cost more when more than about 12 different triangles are on a line.
- How the cutting of a tile into runs overlaps the ISP's work on the next tile ("How fast the ISP's side cuts").
- Why some drains of the span sorter are shaded before the drains in front of them ("In what order the
  TSP shades a tile").
- What the per-primitive resource is that the ISP and the TSP share when 32 rows of 32x1 make a tile cost 1736
  (rows: not a run stage, not a shared setup unit; pfetch: not a parameter fetch or VRAM traffic. In untextured
  rows the ISP keeps pace with the TSP instead of running ahead).
- How the 335 clocks of a tile with nothing to do divide between the start, the ISP, the TSP and the
  hand-overs, and what the 55 to 90 are that a tile with seen polygons costs once.
- How texels are kept: the first tile's cost is small and paid once per render, yet texels 8 apart cost
  2.25 clocks each every time.
- What makes a pre-sorted tile with a translucent modifier volume and an empty opaque list skip its
  background, and what stops it (end of the section on modifier volumes).
- Not measured: the scaler and the strip buffer in the write-out, translucent
  sorting with many layers per pixel, a second console.

## Running it

    bash run2.sh timemeas hw=<ip>             # about 30 seconds; the timing is in the console's log
    tr -cd '[:print:]\n' < /tmp/pvr_tests/timemeas/build/hw.log > fit/timemeas_logs/timemeas_run<n>.log
    python3 pvr-tests/timemeas/analyze.py > pvr-tests/timemeas/tables.txt
    TEST_DEFS="-DTM_ORDER" bash run2.sh timemeas hw=<ip>     # the palette clock scenes only, a few seconds
    TEST_DEFS="-DTM_CUT" bash run2.sh timemeas hw=<ip>       # the cutting scenes only, a few seconds
    python3 pvr-tests/timemeas/order.py --fit fit/timemeas_logs/timemeas_run39_order.log

Earlier logs are kept in `fit/timemeas_logs/before_flat_bg` (Gouraud background, twins by compare NEVER),
`first_flat_bg`, `second_flat_bg`, `before_modvol` (the tile had three lists, not five), `before_pt_modvol`,
`before_tint`, `before_tr_modvol`, `before_bgcol` and `before_keepz`.
