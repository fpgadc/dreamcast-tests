# Probes to do (2026-11-19)

The work list after the SupSample / trilinear / bump-light round (README: supsample .. ssrare). Each item: what is
known, the pixel-level probe (P: the console's picture against refsw3-offline, or the console against itself with
one setting changed), the timing probe (T: timemeas's method, one tile, STARTRENDER to the TSP's end-of-render bit
on the CPU's cycle counter, README "timemeas"), and what it would change.

Step 0, harness: a way to override a pvrsetup.h register for any existing test without editing it
(`TEST_DEFS="-DSETUP_REG=0x030:0x000"` or similar), so that whole existing tests can be rerun with one register
changed and compared console against console. A1, A2, A4 and A5 need it.

Priority, as discussed: A1, then C1 (A1 may explain it), A2, B, then the rest.

## A. Registers refsw3 ignores

All 790 polly2-data dumps (their registers tallied 2026-11-19) and every test use one value for each of these, so
refsw3 models that value only. Only FPU_PERP_VAL is set otherwise by games.

**A1. SPAN_SORT_CFG** (0x030: span sort enable bit 0, offset sort enable bit 8, TSP cache bypass bit 16).
Every dump and test: 0x101. The manual calls it a performance feature ("span sort minimizes TS preprocessing,
offset sort optimizes TSP parameter fetching"), but the span sorter is what forms and orders the runs the TSP
shades (timemeas TM_ORDER: a window of 96 runs, drained one primitive at a time; PIPELINE_SHAPE.md [2b]), and every
TSP value depends on its run (n steps from the run's first pixel; SupSample's half steps even more).
- P: rerun ssrare, ssvol, supsample2, uvrun, uvspan, ptruns, blendpos (the tile buffers), blendsel
  (tspAlphaStuck: order dependent) with 0x000, 0x001, 0x100 (the manual's "setting prohibited"), 0x10101; console
  against the 0x101 run. A difference anywhere means the run model needs the setting.
- T: timemeas's run scenes (runs a line, more than 12 triangles on a line, textured with offset) and TM_ORDER (the
  order of shading) with each setting: is the order raster order without the sorter, what each grouping saves.

**A2. FPU_PERP_VAL** (0x084, "IEEE floating point value for perpendicular triangle compare", default 0).
22 dumps set 0x3c000000 (1/128): floigan_*, nba2k_*, ncaa2k2_*, nfl2k1_*, oogabooga_* (Visual Concepts); all 22
already match the console, so nothing there triggered it. No test sets it.
- P: rerun covmeas's sliver, degen, tiny, lean and skew mixes, the zs25 depth probes, faredge and detinf with 0,
  1/128, 1, 2^20, -1, infinity; console against the 0 run. Then a sweep of its own: triangles turned toward edge-on
  (depth slope 2^-10 .. 2^20 a pixel, area from a pixel to a tile) against each value, to find what is compared
  (a depth slope, the area, the determinant against the depth range) and what happens (culled, depth or colour
  clamped, drawn flat).
- T: the ISP's cost per triangle for edge-on triangles at each value.
- Possibly the open sliver depth (C2): both are almost edge-on geometry.

**A3. TEXT_CONTROL** code book endian (bit 17), index endian (bit 16), bank bit (12..8). Never set by a game.
- P: texfmt / sample variants with each bit, read with address-revealing textures (texgen: every word its own
  index): which bytes the big-endian code book and index orders swap (16, 32 or 64 bit units; 4 bpp nibble order),
  and the address mapping for each bank bit position. Then implement in refsw3's fetch.

**A4. ISP_FEED_CFG: punch-through chunks, the translucent sort cache, and the "ISP out of cache" error.**
Every dump: 0x00800408 (discard 1, punch-through chunk 0x40, translucent cache 512). Known: which primitives go
into a punch-through chunk, and that each chunk is peeled on its own (ptchunk, ptchunkq, ptchunkm: pixels only);
the console reports ISP out of cache (SB_ISTERR bit 0) on the same tiles in every render of 25 of the 64 older dumps
(up to 33 tiles, taxi4) and on two tiles of shenmue_intro, with nothing visible in those pictures (README zdumps).
Not known: what fills the cache, what the core does when it is full, what it costs.
- P: autosort tiles with more layers on a pixel (and more fragments in the tile) than the cache holds, cache sizes
  8, 16, 32 .. 512, discard 0 and 1; console against refsw3, and the console against itself at each size: where a
  full cache changes the picture (layers dropped, drawn unsorted, sorted in batches), and when it does not.
- Interrupts: read SB_ISTERR (and SB_ISTNRM) after each render: the out-of-cache bit against the cache size and the
  layers / fragments / primitives in the tile, to find the count that sets it; the same for punch-through tiles
  against the chunk size; a scene that sets it in exactly one known tile (one tile in the region array at a time).
  Then the dumps that report it: which tiles and why, from their tiles' contents.
- T: render time against the punch-through chunk size (each chunk a peel of its own: cost per chunk and per
  primitive in it) and against the translucent cache size (sorting in batches when it is full?); the time of a tile
  that overflows against the same tile with a cache that holds it; whether the error stops or slows the render.

**A5. FPU_PARAM_CFG burst sizes and thresholds, SDRAM_REFRESH / SDRAM_ARB_CFG / SDRAM_CFG, FB_BURSTCTRL.**
Timing only, by the manual.
- T: timemeas totals with each changed. P: one check that the pictures do not change.

**A6. The strip buffer** (FB_R_CTRL bit 22, strip size 21..16, FB_W_SOF2): measured by sof2sweep, not implemented
in refsw2 or refsw3, used by no dump.
- P: implement from sof2sweep and score it; the oddities as pages of their own (one pixel of the second strip's
  first tile in the first buffer, 16-line strips, tiles column by column, with the scaler on); how the display
  reads strips (FB_R_SOF2) was not looked at; a strip render once left the core stuck for the renders after it.
- T: does the strip mode change the render time.

## B. The YUV converter (TA_YUV_TEX_BASE 0x148, TA_YUV_TEX_CTRL 0x14c, TA_YUV_TEX_CNT 0x150)

Every Sofdec FMV goes through it. nulldc3's emulator has one (libswirl/hw/pvr/pvr_yuv.cpp), refta does not, and no
console test checks the converter itself (yuvbil only reads YUV422 textures). From the manual: YUV_Form 0 =
YUV420 input, 1 = YUV422 input; YUV_Tex 0 = one texture of (U_Size + 1) * 16 x (V_Size + 1) * 16 pixels,
1 = (U_Size + 1) * (V_Size + 1) textures of 16x16; U sizes 16 .. 1024, stride U sizes 32 .. 992 (odd settings
0x05 .. 0x3B, 0x3D); writing TA_YUV_TEX_BASE initializes the converter, which then takes the next input as U data;
TA_YUV_TEX_CNT counts the macroblocks stored and is cleared by that write.
- P: macroblocks of known values through the TA's YUV path, video memory read back (dump32) and compared byte for
  byte with pvr_yuv.cpp: how YUV420's chroma becomes YUV422's (each chroma row twice, or two rows averaged), the
  byte order of the YUV422 words, the layout for YUV_Tex 0 and 1 and each size (twiddled or not), stride sizes,
  the 16x16 textures' order for YUV_Tex 1.
- Edges: TA_YUV_TEX_BASE written in the middle of a macroblock; more macroblocks than the texture holds;
  TA_YUV_TEX_CNT after each one; YUV input between list input in one frame; store queues against DMA.
- T: macroblocks a second (TA_YUV_TEX_CNT polled against the cycle counter), the end-of-conversion interrupt's
  latency, whether conversion holds up list input.

## C. Open pixel-level loops

**C1. ssrare: SupSample points half a line down, one grid unit high on some planes.** 24 pixels of 6.1 million
(README ssrare); per plane (hits cluster by tile), those planes' B / 2 is 0.69 .. 0.94 of a unit above the grid;
the rarer condition is unknown; eight arithmetic variants ruled out.
- P: read the value at the hy = 1 points directly, plane by plane: a texture that is zero but for one row, and V
  planes such that only the points half a line down land on that row (the average then is that point's texel / 4);
  a coordinate ramp in that row reads the point's value to a fraction of a unit. Sweep the plane's B / 2 and C
  fractions, the line, the run start. Compare with the next line's whole-step value (does the TSP step from it?).
- A1 first: if the span sorter's grouping changes these points, the cause is in how the TSP walks a span.

**C2. Sliver depth (zs25, the ISP)**: 619 of 7,795 pixels of slivers with a nonzero determinant and different
corner depths outside the console's 0.1-wide brackets. First, from the archived data (no console): do the misses
cluster by triangle, a whole plane a unit off as in C1? Then probes from what that shows. A2 may bear on it.

**C3. Sliver colours**: the TSP colour model gives 0 at 870 sliver pixels a console shows (a determinant of a few
units: division by almost zero, 1/w below zero unmeasured); 4 sliver pixels with a negative model depth are drawn.

**C4. The regression's known misses**: detinf 478 / 480 (stored depth below 2^-125), faredge 1245 / 1340 (pages 8 to
11, the K 127 vertical triangles), modvol 187 / 192 (two-volume cells at extreme depth), hugepos from 2^114 and the
translucent sort's cross-talk with garbage depths (from 2^62). Each has its README section.

**C5. The dumps that missed: done 2026-11-19, all 786 polly2-data dumps (and the 63 older scenes) are the console's.**
f1wgp_driver 5, f1wgp_race 2, ff2_fight 1, vrally_rally4 1 pixels were two punch-through rules (README optcull,
ptchunkcost, ptchunklag): OpInPt is "the tile's punch-through list holds a primitive", drawn or not (ff2_fight's tile
had two culled triangles); a culled primitive takes no room in a chunk, but the next one is checked with 3 vertices
more (f1wgp, vrally). The way there: dumprender partial tiles in 32 bits, refsw3 with run starts forced (the forced
starts pointed at the right pixels but at the wrong cause, a chunk carry: ptchunkrun_races / ptchunkcarry_races came
out of that and stand on their own), then switching rules per triangle tag until one fit every tile, then a console
test per rule. Open from here: a culled quad right after a drawn quad; a primitive with an infinite or NaN corner in
a chunk (counted as stored, not measured).

**C6. Translucent modifier volumes**: not covered (the modvol test's translucent modifier list blanked a console's
frame and was left out). Whether any dump uses them: not checked.

**C7. Render to texture** and its 1555 / 4444 formats: not covered.

**C8. tspAlphaStuck** (blendsel): not measured with textured polygons, what else clears it in a tile, the
automatic sort.

**C9. HALF_OFFSET** with its half-pixel bits off: how the TSP samples (only measured at pixel centres); with
SupSample.

**C10. SupSample with the rest**: punch-through in a modifier volume (the PT_MOD pass), translucent modifier
volumes, sprites, 16 bit UVs, palette and YUV textures, render to texture, DCalcCtrl with perspective.

**C11. Dumps not run**: the 4 that keep a console busy for minutes (garou_intro, garou_title, gunbird2_title,
td6_menu: what keeps it busy, see T below) and the 65 old-format dumps.

**C12. refsw2** (the emulator's renderer): its SupSample, trilinear and bump-light ports are unverified; its mipmap
level is a rough estimate (no console test is close); no strip buffer. Score it against the archived tests once
its mipmap level follows refsw3's.

**C13. The core's persistent offset colour (found by purist, 2026-11-19).** A bump map drawn WITHOUT the offset bit
lights itself with the offset colour (K1 K2 K3 Q) the core used last: the core keeps the last offset colour, and it
survives taking the power off (purist: 2 seconds, 30 seconds, 5 minutes unplugged; the value was still there).
refsw3 keeps one static offset colour (`offs` in refsw_tile.cpp, 0x20004080 at start, an arbitrary choice) that every
pixel with the offset bit overwrites and a bump map without the bit reads. Not measured:
- P, which value persists: the last pixel shaded with the bit (its interpolated offset on a Gouraud polygon, or the
  vertex's), the last SupSample point, a bump map's raw light; and "last" in the TSP's order, i.e. the span sorter's
  (SpanGen / SpanSort): a tile of several offset polygons, then a bump map without the bit, the polygons sent in
  different orders and placed so that the span sorter shades them in another order than the list.
- P, how long and through what: across tiles, renders, frames, SOFTRESET (core and TA), a new region array, the
  display off; its value after a cold boot (purist: it survives one; is there a default when the console has been
  off for longer, hours?).
- P, who else reads it: vertex fog (the offset colour's alpha) on a polygon without the bit; decal / modulate
  without the bit (by the manual nothing is added; check that nothing is); two-volume polygons, the second
  volume's offset; intensity shadows.
- For the emulator (purist's suggestion): a random value at start-up, then the last one set kept at run time.

## D. Timing probes

- **D1. SupSample's cost**: per pixel and per run against the same scene without it (four points: four times the
  TSP's work, or a pipeline that hides some), with point, bilinear, mipmapped, fog, offset.
- **D2.** Trilinear (two passes: twice the scene), two-volume polygons inside and outside a volume, bump maps
  (bumphang: they stop the render sooner), the punch-through passes.
- **D3.** The span sorter settings (A1) and the TSP cache bypass, SPAN_SORT_CFG bit 16 and the per-polygon
  CacheBypass bit: parameter fetch cost per primitive with and without the cache.
- **D4.** FPU_PERP_VAL (A2): the ISP's cost per edge-on triangle.
- **D5.** The YUV converter's throughput (B).
- **D6.** The busy dumps (C11): replay each on a console (pvr-renderer) and time STARTRENDER to the end of render;
  which tiles take the time (one tile at a time in the region array).
- **D7.** SDRAM and burst settings (A5).
- **D8.** Chunked punch-through and the translucent sort cache against the ISP out of cache error (A4); and when a
  chunk's resolved pixels carry into the next chunk's runs (ptchunkcarry_races: by where the ISP is against the TSP).
