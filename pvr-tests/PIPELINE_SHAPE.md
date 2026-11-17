# The PVR core's pipeline (ISP, TSP, write-out): a best guess

Written 2026-11-17 from the core tests in this folder. The arithmetic comes mostly from covmeas, zmeas, zblock,
ztile, uvmeas, uvspan, uvrun, wmeas, cmeas, mipmeas, planes, bumpfn, yuvbil, blend*, fbpack and accbuf; the timing
from timemeas, whose report is [timemeas/REPORT.md](timemeas/REPORT.md). Also from refsw2 in nulldc3
(`libswirl/rend/refsw2`: pvr_softfp.h, pvr_isp_edge.h, pvr_isp_depth.h, pvr_tsp_plane.h, refsw_tile.cpp,
refsw_lists.cpp), which gives the console's picture on every regression test except the timing-dependent cells of
accbuf and blendpos, 5 regions of modvol, 2 of detinf, the far-edge cases of faredge (95 regions) and one
bracket of zmeas.

Nothing here was seen inside the chip. It is the cheapest hardware that gives the behaviour seen from outside.
Each claim carries a tag:

- **[M] measured**: a console reading or timing shows it directly (test names in brackets)
- **[I] inferred**: the simplest structure that explains several measured behaviours at once
- **[G] guess**: fits the data, but other structures would too

Times are in core clocks (100 MHz, 10 ns). The tile is 32 x 32 pixels.


## Overview

```
 STARTRENDER
   |
   v
 [1] tile sequencer: region array entry (6 words): tile, five list pointers, flags (keep Z, no write-out,
   |    pre-sort); the background polygon (ISP_BACKGND_T/D) for every entry
   v
 [2] ISP: object list walker -> parameter fetch -> ISP setup (edges, depth plane; 24-bit truncating softfp)
   |    -> rasterizer, ONE TILE LINE (32 px) A CLOCK -> depth buffer (19-bit mantissa) + tag buffer
   |    + modifier volume bits; passes: opaque, punch-through, translucent (pre-sorted or sorted per pixel)
   v    run queue, ~512 runs: the ISP runs up to ~15 background tiles (32 runs each) ahead of the TSP
 [3] TSP setup: per primitive seen, planes 1/w, ARGB, U/w, V/w, offset ARGB (16-bit floats); mip K;
   |    runs ahead ~11-12 primitives, across tiles
   v
 [4] run start: first value of each run C + floor(A x0) + floor(B y) on the tile's grid; shares an
   |    arithmetic unit with setup; queue of ~150-200 pixels after it
   v
 [5] pixel pipe, ONE PIXEL A CLOCK: plane step -> divide by 1/w (table reciprocal) -> texture address,
   |    mip level -> texel fetch (four 16-bit words a clock) -> decode (twiddle, VQ, palette, YUV->RGB, bump)
   |    -> filter -> shade, offset, fog -> blend (8-bit products, each shifted) -> PT alpha test
   v
 [6] tile buffers: EXACTLY TWO 32x32 colour buffers; the one not being shaded into is being written out, and
   |    is also the "secondary accumulation buffer"; they swap when the TSP starts the next tile
   v
 [7] write-out: pack (565/1555/4444/888/0888/8888), dither, scaler (vertical filter streaming through the
        tiles), FB_X/Y_CLIP, strip buffer (SOF1 / SOF2); 1045 (16 bpp) / 1105 (888) / 1357 (32 bpp) a tile
```

Stages of consecutive region array entries overlap, so in a render of many tiles each tile costs about the
largest of its ISP, TSP and write-out times [M timemeas].


## [1] Tile sequencer

- The region array is walked in order. Every entry is a tile to the whole pipeline, even one that draws nothing
  and is not written out (KallistiOS's dummy first entry). Its background polygon is still shaded: 1035 clocks,
  about 1.01 a pixel [M timemeas "fixed costs"].
- **Keep Z** (entry bit 30) skips both the Z clear and the background polygon. A tile with background costs
  1177 = 1035 shading + 142 Z clear and setup. A tile with nothing to do costs 335 plus its write-out [M timemeas
  keepz].
- Each of the five list pointers present costs the ISP 20 to 31, the modifier volume lists too [M timemeas].
- ISP_FEED_CFG bit 0 does not matter with 6-word entries [M timemeas].


## [2] ISP

### Setup (one triangle per ~16 clocks)

- **Cost [M timemeas ISP]:** 16 clocks a triangle whatever its format, depth result, or place in the tile.
  - **Strips:** after the first triangle, alternately 19 and 15.
  - **Sprites:** 19.
  - **Culled** (winding or too small): 15.
  - **Size:** counts only past 16 lines, at +1 a line per triangle. A 32x32 quad takes 64, a 32x32 sprite 32.
  - **[I]** A setup stage of about 16 clocks per triangle feeds a rasterizer that does one 32-pixel tile line
    per clock. The rasterizer's first 16 lines hide behind the next triangle's setup.
- **Arithmetic [M covmeas, zmeas, precision]:** one soft-float with 24-bit truncating mantissas, on absolute
  screen coordinates.
  - The corners are taken in the order (v1, v3, v2).
  - The determinant is summed (e1 + e2) + e3. The depth plane's slopes are summed (t1 + t2) + t3, its constant
    (t1 + t3) + t2.
  - Each edge's constant is c = T(xb ya) - T(xa yb).
  - The determinant's reciprocal is truncated, and the depth plane is in determinant form.
  - Exponent field 0 is zero (denormals too) and 255 is infinity (NaNs too); results of 2^128 or more saturate
    to infinity. Measured number by number for the TSP's setup [M uvmeas wexp*]; **[I]** the same for the ISP,
    since huge is 250/250 with them.
  - **[G]** One multiply-add datapath, used about a dozen times per triangle in a fixed sequence, fits the
    ~15-clock budget.
- **Winding** is the sign of the setup determinant, not of the exact area. A determinant of exactly 0 counts as
  positive [M covmeas sliver, degen].
- **Degenerate triangles are drawn when culling is off** (a hardware bug the model reproduces) [M covmeas degen,
  tiny]:
  - an edge between two equal corners is zero everywhere and counts as inside, so three equal corners fill the
    whole 32-pixel width of their tile rows;
  - a determinant of 0 gives the mean of the three corner depths, and the TSP takes the third corner's colour.

### Rasterizer: coverage

- **Per edge, per tile [M covmeas, 55M pixels].**
  - The value at the tile's first sample is kept as a 20-bit float.
  - The two slopes are 20-bit floats, cut to 21 bits under the larger one's exponent.
  - A shared unit G = 2^(E-27), where E = max(exp(larger slope) + 6, exp(start) + 1), and "zero" means
    |v| <= 64 G.
  - **[I]** The edge is then stepped in fixed point on that grid. That is a block floating-point format per edge
    per tile, which makes the per-pixel work adds only.
- **Decision rule [M].**
  - Take the row's value at the tile's first column. If it is "zero", the whole row of the tile gets one answer,
    from the sign of the column step (or of the row step when the column step is "zero").
  - Otherwise each pixel takes its own sign. A zero pixel is inside if the next pixel to its right is not
    negative, and at column 31 the column step decides.
  - So nearly horizontal edges switch on whole tile rows, and shared edges can draw a pixel twice.
- **Saturation [M hugepos, faredge].** The tile-start value saturates to +-2^127 when the sum dx y0 - dy x0 or
  the start reaches 2^128. Exception: with a far y of 2^127 or more, the edge behaves as if its constant were
  about +-2^132.06 (not modelled).
- **Row range [M covmeas zprobe].** Polygons are rasterized only on rows floor(ymin) .. floor(ymax + 0.5) of
  their corners (the same row numbers with HALF_OFFSET); outside them nothing, depth included. Modifier volume
  triangles have no row range.
- **[G]** One line per clock means 32 pixels x 3 edges of sign tests per clock: 96 adders, or 3 adders and
  32-wide comparisons against a precomputed row.

### Rasterizer: depth

- **Block floating point per polygon and tile [M zblock 59220/59220, ztile 11280/11280, zmeas 50279/50280].**
  - The exponent is that of the depth at the tile's first pixel centre, taken again in every tile, even where the
    polygon does not cover that pixel.
  - Start = ((a x0 + b y0) + c) x 1/det, truncated to 20 bits under that exponent.
  - The slopes are 20-bit floats that share the larger one's exponent (21 bits under it), then cut to 26 bits
    under the start's exponent.
  - The depth is stepped in fixed point, then normalised to a 19-bit mantissa on its own exponent to be compared
    and stored.
- **Reach [M zmeas reach].** The stepping is fixed point at least 29 powers of two above the start's exponent,
  with no overflow or saturation: at least 26 + 29 + 1 bits plus sign. A start of exactly zero is stepped from
  zero the same way, on the slopes' own grid (21 bits under the larger slope's exponent).
- The depth buffer stores and compares exactly what setup computed. An equal depth compares as less [M
  precision, depth].
- **[I]** A stored depth is about 28 bits: an 8-bit exponent and a 19-bit mantissa (plus a sign?). ISP_BACKGND_D
  reads back with its low 4 bits zero (23 - 4 = 19) [M regdump].
- Depth processing is invisible in the timing: failing, passing or writing all cost 16 a triangle [M timemeas].

### Passes, sorting, modifier volumes

- **Opaque list** [M timemeas modifier volumes]:
  - one pass;
  - a modifier volume costs ISP time only (65 for a 2-triangle volume);
  - inside, the second volume's parameters replace the first's;
  - the part of a polygon inside a volume is its own triangle to the TSP (its own setup and runs).
- **Punch-through list.** Pixels inside an opaque modifier volume are shaded twice, the second time with the
  second volume's parameters and blend mode. Pixels failing the alpha test are shaded later as runs of their own
  [M timemeas, uvrun]. Untextured punch-through polygons are drawn at every pixel, even below PT_ALPHA_REF
  [M fbpack].
- **Translucent list.** A pre-sorted tile (region entry bit) is shaded in list order. A tile sorted per pixel is
  shaded in layers, back to front. A translucent modifier volume works only in a tile sorted per pixel [M
  timemeas tr_modvol, uvrun].
- **ISP ahead of the TSP: a queue of runs [M timemeas ispq].** The ISP's end-of-render bit is not the end of the
  ISP's work. In a render of 16 or more background tiles it comes 19980 clocks (14.6 tiles) before the TSP's bit.
  - When the ISP ends, about 430-480 runs are still to be shaded, whatever the tiles hold. A background tile is
    32 runs, hence ~15 tiles.
  - Not time: the 565 framebuffer gives the same 14.6 tiles. Not tiles: 4 columns of 8x32 a tile lead by 4.6.
    Not primitives: 4 rows of 32x8 lead by 14.5, where 4 columns of 8x32 (same primitives, 4x the runs) lead by
    4.6.
  - **[I]** A queue of about 512 runs between ISP and TSP. Each entry is probably a parameter address, a line and
    an x range, about 40 bits, so about 2.5 KB. Not known: whether a run cut in two by a nearer polygon counts
    twice.
  - With one dummy entry in front, the first ~1000 clocks of the tile's ISP work are free.


## [3] TSP setup (one primitive seen per ~39-64 clocks, running ahead)

- **Cost per seen triangle [M timemeas]:** at least 39 clocks untextured, 48 textured (any format or filter, fog
  included), 64 textured with an offset colour, 58 in the punch-through list, 39 translucent. A triangle takes
  the larger of this and the time of its runs.
- **Planes [M planes, uvmeas]:**
  - untextured: 1/w and A, R, G, B (5 planes; flat polygons too, as three equal corners);
  - textured: also U/w and V/w (7);
  - with offset: also the offset's A, R, G, B (11).
  - Every plane comes from the same 24-bit setup. An attribute plane uses the 1/w plane's products times the
    corner's attribute; its constant is summed (t1 + t2) + t3. Coefficients are cut to 16-bit floats toward
    zero [M uvmeas].
- **Offset colour of 0 costs nothing [M timemeas vol2off, offs3].** All four offset planes are dropped together
  when every offset colour the triangle's shading uses is 0. That is the three corners for Gouraud, only the
  flat-shading corner for flat.
- **DCalcCtrl 1** costs 5 clocks more per seen triangle in every format, textured or not [M timemeas dcalc].
- **Mip level [M mipmeas, 20.7M values].** Per polygon, K = max of the two lengths (derivative numerators from
  the uncut coefficients; sqrt(a^2 + b^2) = larger + 27146/65536 of the smaller) x size x MipMapD/4, cut to
  8 bits. DCalcCtrl 0 takes the numerators at the screen origin, 1 at the first vertex. Per pixel: w cut to
  8 bits, w x w cut to 8 bits, K x w^2 cut to 8 bits. The level is the exponent and the fraction is the next
  7 bits. **[I]** Two 8-bit multiplies and a leading-one search per pixel; not a log domain (the w x w cut is
  the fingerprint).
- **Look-ahead [M timemeas ahead, ahead2].** Setup runs about 11-12 primitives ahead of the pixels, across
  primitives and across tile boundaries. Its cost shows only when many small primitives come close together.
  **[I]** A store of about 12 set-up primitives (planes, mip K, parameters). It is limited by storage, not by
  time.


## [4] Run start

- **What a run is [M uvspan, uvrun]:** a maximal stretch of pixels of one triangle on one line of a tile, in one
  pass, with the same modifier volume state. Runs restart at a hidden pixel (opaque list), at a volume boundary,
  at a punch-through pass boundary, and at a layer boundary in a translucent tile sorted per pixel. They are not
  cut at fixed columns: up to 32 pixels.
- **Value [M uvmeas, 23.6M readings, tspprobe]:** first value = floor(C) + floor(A x) + floor(B y), in screen
  coordinates (half pixels included). Each product is cut to the tile's grid on its own before the integer add,
  so it is a truncating multiplier plus an integer adder (fused or rounded variants are excluded). The grid is
  22 bits under E = max(exp(C), exp(A) + exp(X) - 1, exp(B) + exp(Y) - 1), where X and Y are the far half of the
  tile: the exponent a product can reach in the tile. Then floor(A) per pixel.
- **No zero [M uvmeas wexp*, wzero*].** A zero coefficient counts as +-2^-127, an infinite one as +-2^-128 (the
  signed 8-bit exponent wraps). The value's exponent clamps at -128.
- **Cost [M timemeas spanfmt, spanshad]:** a sustained run start costs 3.9 clocks untextured, 4.2 textured,
  6.3 with an offset colour. Flat and Gouraud are the same to the clock, so the cost is not per plane. Runs of
  different triangles: at least ~4.7 / 6 / 8.
- **Shared unit [M timemeas share].** On lines paced by run starts, a new triangle's setup takes ~2 clocks a
  plane out of the run starts (13.7 for 7 planes, 22.8 for 11). **[I]** Setup's per-plane work and run starts
  share one arithmetic unit: about 4 products a clock, about 9 products a plane at setup, 2 per run (A x and
  B y). The multiplier count can't be determined from timing.
- **Queue after run start [M timemeas span].** Bursts of run starts are hidden, about 45-70 runs (~180-290
  clocks) per tile, whatever the length of the runs queued before them. **[I]** A queue of about 150-200
  pixels of shading between run start and the pixel pipe.
- **Crowded lines [M].** With more than ~12 different triangles on a line, in the opaque pass only, runs cost up
  to twice as much. It is the number of triangles, not of runs. **[G]** A small store of set-up triangles per
  line (the same ~12 as the setup look-ahead) that thrashes: 1x8 sprites pay about twice the DCalcCtrl step.


## [5] Pixel pipe (one pixel a clock)

- **Fill rate [M timemeas]:** 1.01 clocks a pixel for untextured, point-sampled formats (VQ, YUV, palettes with
  16-bit entries, not twiddled), Gouraud, offset and fog. Twiddled 16-bit bilinear 1.11; mipmapped bilinear 1.37.
- **Plane value per pixel [M tspprobe wfixed=].** The value is normalised before it is cut to 16 bits: a
  leading-one count and shift on the full grid integer, then the cut. Even a fixed 16-bit field dropping one low
  bit is wrong on 30,852 of 23.6M readings.
- **Divide by 1/w [M uvmeas, 5.46M divisions].**
  - Multiply by a table reciprocal of the 16-bit divisor mantissa D: r = (64 t[i] - 1 - n[i] (D & 63)) >> 7 with
    i = D >> 6. The table has 512 entries, with c[i] in {0, 1, 2} read off the console and no formula found, so
    1/1.0 = 65535/65536.
  - The product is floored at 17 fraction bits, then normalised.
  - A negative 1/w is not negated properly: the leading bit is kept and the 15 bits under it are negated.
  - Colours are fractions in the planes (8-bit value x 2^-8); the result is floor(v + 0.5) [M cmeas].
  - **[I]** Per pixel: one reciprocal table and one 16x16 multiplier for each perspective attribute: U, V, four
    colour channels, and four offset channels when offset is on.
- **Texture fetch [M timemeas align2, yuvbil].**
  - Four 16-bit words come in per clock, and words that came in for the pixel before don't count again.
  - A palette with ARGB8888 entries pays exactly one more clock a pixel, point or bilinear.
  - YUV422 bilinear pays one more (two more at an odd x that shares nothing with the pixel before).
  - Bilinear on a texture that is not twiddled pays about one more (2.07).
  - Texels 4 or more apart are bound by the memory at 2.14 clocks.
  - **[I]** A 16-bit path per filter tap: a 32-bit texel takes two steps. A non-twiddled 2x2 footprint spans two
    memory rows.
  - Ruled out: two converters that take any two texels, one converter per U/V pair, and one 64-bit word of four
    texels a clock.
- **Texture memory caching [M timemeas].** The first tile costs +35 to +455 clocks once per render, but texels
  far apart cost 2.25 more every time. **[G]** A small cache of recent words. Its organisation is not known.
- **Decode [M texfmt, sample, yuvbil, bumpfn].**
  - Texels widen by bit replication; strided bilinear 565/1555/4444 widen by shift only.
  - YUV is converted to RGB per texel before bilinear, and U/V pair by x even when twiddled.
  - Bump maps: I = K1 + ((K2 sin[S]) >> 8) +- ((((K3 cos[S]) >> 8) cos[4x]) >> 8), from two measured 256-byte
    tables. R is filtered the short way round the circle.
- **Filter [M]:** bilinear is two truncated 5-bit steps. A trilinear pass fades by the level fraction (2 x
  fraction, 127 -> 256). A mipmapped texture whose declared V size differs from U forms V with the declared size
  and then scales it.
- **Shade, offset, fog [M env, fog, tspw]:** modulate is (a x b) >> 8 with no correction. The fog table is
  indexed with the TSP's own 1/w. Cheap-shadow intensity scales the colour before it is clamped [M faredge].
- **Blend [M blend 4096/4096]:** out = sat((src x ks >> 8) + (dst x kd >> 8)). Each factor is k = x' or
  256 - x', where x' = x except 255 -> 256, and each product is shifted on its own. **[I]** Two 8x9-bit
  multipliers per channel, 8 a pixel. SrcSel and DstSel both set: the polygon writes nothing.
- **Sampling points [M halfofs 1080/1080]:** HALF_OFFSET bit 0 moves the ISP's coverage sample, bit 1 the TSP's
  colour/UV sample, bit 2 the texel (bilinear only). The TSP evaluates 1/w again itself, following its own
  sample bit, rather than taking the ISP's depth [M tspw, halfofs].


## [6] Tile buffers

- **Exactly two 32x32 colour buffers [M accbuf].** The one not being shaded into is the one being written out,
  and it is also the "secondary accumulation buffer" of the blend's select bits. A DstSelect write shows up in
  the next tile's buffer, and SrcSelect reads it. They swap when the TSP starts the next tile, which is also
  when that tile's write-out starts. A tile that is not written out does not swap.
- **Write waits, read doesn't [M accbuf, blendpos].** A polygon that writes the secondary buffer waits until the
  previous tile's write-out is done (~1025 clocks for a tile's worth). One that reads it doesn't wait, and
  corrupts the first lines of the picture being written out (nondeterministic).
- **Stuck alpha (a hardware bug) [M blendsel].** After a polygon with SrcSel and DstSel, SrcInstr ONE or SRC_A
  and DstInstr ZERO, every later polygon in that tile has its own alpha replaced by 0 unless it is 255.
- **Empty keep-Z tiles [M accbuf].** Two in a row (written out) hang the render until a core reset. One followed
  by other tiles costs ~2390 clocks and a neighbour gets the other buffer's picture. **[I]** An empty tile's end
  comes at once and doesn't wait for the previous write-out: a handshake collision.


## [7] Write-out and scaler

- **Cost [M timemeas]:** 1045 clocks a tile at 16 bpp, 1105 at 888, 1357 at 0888 or 8888, even when the clip
  removes every pixel. **[I]** About one pixel a clock, bound by VRAM bandwidth at 32 bpp (4 KB in 1357
  clocks).
- **Formats [M fbpack, 5,222,400/5,222,400].** 0888's top byte is fb_kval. 0555's bit 15 is fb_kval bit 7.
  1555's bit 15 is alpha >= fb_alpha_threshold. 4444 cuts alpha too, and alpha is dithered. The dither is one
  4x4 table of 16 levels added before truncation, saturating (>> 1 for 5 bits, >> 2 for 6). 24 and 32-bit
  formats are not dithered.
- **Scaler [M scalermeas, scalertile].**
  - Vertical: row j sits at j x factor (1/1024 lines), blended with a 4-bit fraction. There is a 1-2-1 filter
    only when the factor is above 0x400, and the row counter is 10 bits.
  - Horizontal: a pair average.
  - The scaler is a stream through the tiles in region array order. A tile with y = 0 restarts it, and other
    tiles continue the stream at their own x. **[I]** One line of vertical-filter state per framebuffer column,
    so only column-major order (KallistiOS) is safe when the factor isn't 0x400.
  - Hardware bug: with the filter, the last row of the last tile has pixels 1..31 stuck at pixel 0's value.
  - Changing SCALER_CTL between renders stalls the next render unless the core is reset first.
- **Clip and strip buffer [M sof2sweep, sof2field].** FB_X/Y_CLIP clip the scaler's output, inclusive.
  FB_W_SOF2 is the second strip buffer: with FB_R_CTRL bit 22, strips (size in bits 21..16, in units of 16
  lines) go alternately to SOF1 and SOF2.
  It is never written otherwise.


## Throughput budget

| work | clocks |
|---|---|
| ISP: triangle (strip: alternately 19 / 15; sprite 19; culled 15) | 16 |
| ISP: line beyond 16, per triangle | 1 |
| ISP: list pointer present, each | 20-31 |
| tile: nothing to do / Z clear and setup / background shading | 335 / 142 / 1035 |
| TSP setup per seen triangle: untextured / textured / offset / PT | 39 / 48 / 64 / 58 |
| DCalcCtrl 1, per seen triangle | +5 |
| run start (sustained): untextured / textured / offset | 3.9 / 4.2 / 6.3 |
| pixel: most formats / twiddled bilinear / mip bilinear | 1.01 / 1.11 / 1.37 |
| pixel: 8888 palette, YUV bilinear, non-twiddled bilinear | 2.0 |
| pixel: texels far apart | +2.25 (memory bound 2.14 at 4 apart) |
| write-out: 16 / 24 / 32 bpp | 1045 / 1105 / 1357 |
| look-ahead: setup / run start / ISP | ~12 primitives / ~150-200 pixels / ~512 runs (~15 background tiles) |


## Storage, roughly

| what | size |
|---|---|
| ISP depth buffer, 32 x 32 | 1024 x ~28 bits (8-bit exponent, 19-bit mantissa) |
| ISP tag buffer + stencil / modifier bits | 1024 x (parameter address + a few bits) |
| ISP to TSP run queue | ~512 runs x ~40 bits (parameter address, line, x range) = ~2.5 KB [I] |
| TSP primitive store | ~12 x (11 planes x 3 x 16-bit floats + mip K + texture/blend words) [G] |
| run queue | ~150-200 pixels |
| TSP reciprocal table | 512 entries (t[i], n[i], c[i]) |
| bump tables | 2 x 256 bytes |
| fog table | 128 entries (FOG_TABLE) |
| palette | 1024 entries (PALETTE_RAM) |
| tile colour buffers | 2 x 1024 x 32 bits |
| dither table | 16 x 4 bits |
| scaler line state | one line of the framebuffer (vertical filter) |


## Hardware bugs the tests found (all reproduced by refsw2 except where noted)

- Degenerate triangles fill whole tile rows when culling is off [covmeas degen].
- Stuck alpha after a SrcSel+DstSel polygon [blendsel].
- Reading the secondary buffer races the write-out (nondeterministic; not modelled) [accbuf, blendpos].
- Two empty keep-Z tiles in a row hang the render [accbuf] (not modelled).
- A non-twiddled bump map with bilinear stops the render at the 16th such quad shaded (8th in the opaque and
  punch-through lists). The count carries over between renders and a core reset clears it [bumphang] (not
  modelled).
- The scaler's filtered last row of the last tile is stuck [scalertile].
- A negative 1/w is negated wrongly in the divider [uvmeas wneg].


## Open questions

1. **ISP to TSP run queue:** what an entry holds, and whether a run cut in two by a nearer polygon counts twice.
   Also why 32 sprites of 32x1 a tile cost 1736 a tile in a render of many tiles: more than either unit's own
   cost (timemeas ispq). **[G]** 1736 / 32 = 54 = the TSP's 39 + the ISP's ~15 per primitive, as if the ISP and the
   TSP set up primitives in one engine (they use the same 24-bit setup arithmetic); a format sweep would tell
   (39 / 48 / 64 + ~15) from a span sorter's cost per primitive change (the same for every format).
2. **Two depth pixels one step high** (zmeas zero6, reach1r cell 227): both a negative start with the plane
   crossing zero in the tile, the exact sum just under a stored boundary.
3. **Crowded lines:** why runs cost up to twice as much with more than ~12 different triangles on a line, opaque
   pass only.
4. **Texture caching:** its organisation. The first tile pays +35..+455 once per render; texels far apart pay
   2.25 every time.
5. **The fixed 335 clocks** of an empty tile, and the 55-90 a tile with seen polygons costs once: how they
   divide between start, ISP, TSP and hand-overs.
6. **Multiplier count** in the TSP's shared setup / run-start unit. Timing gives ~4 products a clock but cannot
   tell the multipliers apart.
7. **Sliver depth:** slivers with a nonzero determinant and different corner depths get garbage depth and colour
   (zmeas zs25: 619 of 7795 pixels outside the brackets).
8. **Far edges:** the ~2^132 edge constant when a far y reaches 2^127 [faredge].
9. **The pre-sorted translucent-modifier glitch:** a pre-sorted tile with an empty opaque list and a translucent
   modifier volume skipped its background. It is history-dependent [timemeas].
10. **Untimed:** the scaler and strip buffer in the write-out, translucent sorting with many layers per pixel, and
   the TA writing VRAM during a render.
