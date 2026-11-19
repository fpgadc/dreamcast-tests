# Probes to do (2026-11-19)

The work list after the SupSample / trilinear / bump-light round (README: supsample .. ssrare). Each item: what is
known, the pixel-level probe (P: the console's picture against refsw3-offline, or the console against itself with
one setting changed), the timing probe (T: timemeas's method, one tile, STARTRENDER to the TSP's end-of-render bit
on the CPU's cycle counter, README "timemeas"), and what it would change.

Step 0, harness: done 2026-11-19: `bash run2.sh <test>+<reg>=<value> hw=<ip>` reruns a whole test with every write
of that register forced (pvrio.h REG_OVERRIDE), into /tmp/pvr_tests/<test>+<reg>=<value>, next to the plain run.

Priority, as discussed: A1, then C1 (A1 may explain it), A2, B, then the rest.

## A. Registers refsw3 ignores

All 790 polly2-data dumps (their registers tallied 2026-11-19) and every test use one value for each of these, so
refsw3 models that value only. Only FPU_PERP_VAL is set otherwise by games.

**A1. SPAN_SORT_CFG** (0x030: span sort bit 0, offset sort bit 8, TSP cache bypass bit 16). Done 2026-11-19
(README spansortcfg): no setting changes a picture (14 run-sensitive tests and timemeas's pages, console against
itself); offset sort makes a strip's triangles one primitive to the sorter (modelled in refsw3's SpanSort); the
sorter off gives near raster order with timing-dependent early runs (unmodelled: needs the clocked pipeline) and up
to 4.6x the time (the TSP keeps 16 primitives set up); bit 16 changes neither order nor time (2%); 0x100 and 0x10001
hang the core until power-off. Left: the pictures with bit 16; the sort-off order (a clocked model); what in a render
sets off the two hangs (each costs a power cycle).

**A2. FPU_PERP_VAL**: done 2026-11-19 (README perpval): a triangle with |det| <= |FPU_PERP_VAL| is drawn as one of
determinant zero (mean depth, flat colour at the third corner); sign ignored; modelled, exact at 1/128, 1, -1, 2^20 on
16 sets. Left: an infinite value on huge or infinite determinants (faredge, detinf); the mean's arithmetic (zmeas gen5
at 2^20: 3,807 of 174,646 pixels, the dataset to pin it); the ISP's cost (D4). A determinant equal to it counts (perpeq, 1800 of 1800).

**A3. TEXT_CONTROL**: measured 2026-11-19 (README texctl): bit 16 moves VQ index reads (^8) and code book entries
(^1); bit 17 swaps the bytes of every 16-bit unit of texture data in every format; the bank bits move VQ reads only.
Left: model them in refsw3's fetch (agents/nulldc3/prep/texctl.md), then archive texctl; the bank bits' exact mapping;
the 16 address bits above 128 KB.

**A4. ISP_FEED_CFG: punch-through chunks, the translucent sort cache, and the "ISP out of cache" error.** Partly done
2026-11-19 (README ispcache): out of cache from size / 8 full-tile layers (sizes 64-512; 32 and below as 32), twice
that with 8x8 triangles, never at 256 layers; the picture unchanged down to size 64 (refsw3's), wrong below (not
modelled). Left: what the cache holds exactly (fragments, primitives, per pixel or per tile), the picture with a
small cache, the time against cache and chunk sizes (D8), the punch-through chunk's error bit.

**A5. FPU_PARAM_CFG burst sizes and thresholds, SDRAM_REFRESH / SDRAM_ARB_CFG / SDRAM_CFG, FB_BURSTCTRL.**
Timing only, by the manual.
- T: timemeas totals with each changed. P: one check that the pictures do not change.

**A6. The strip buffer**: modelled 2026-11-19 (README scalermeas/sof2sweep): the write-out's growing strip number;
10 of sof2sweep's 11 strip pages exact but for one pixel each. Left: that pixel, half-tile strips with the tiles
column by column, strips with the scaler, where lines before a buffer go, how the display reads strips.

## B. The YUV converter: measured 2026-11-19 (README yuvconv)

nulldc3's pvr_yuv.cpp is the console in every ordinary case (YUV420 and YUV422, one texture of any size or 16x16
textures, one macroblock too many into 16x16). Left: one macroblock too many into a larger texture (32x16: 1,024
bytes), TA_YUV_TEX_BASE written mid-macroblock (1,534 bytes); a converter in refta (so that the offline renderer can
run yuvconv); input by DMA, mixed with list input; throughput (D5).

## C. Open pixel-level loops

**C1. ssrare: done 2026-11-19** (README ssrare): a SupSample point half a line down has floor(C + B / 2), not
floor(C) + floor(B / 2); ssrare and sscolor exact. Found from the archived pages alone (one unit more per plane,
labelled by the console, then grouped by each term's fraction). Confirmed by pvr-tests/sshalf: the console has the
new rule on all 728 pixels where the rules differ.

**C2. Sliver depth (zs25, the ISP)**: done (2026-11-19): refsw3 is the console on every zs25 set and the finer
zs25f_* ladder (archive_check). covprobe's misses were not depth: the console's depth buffer under them is refsw3's
(pvr-tests/probes_zs25z, 20,480 of 20,480), the pixels are the TSP's garbage colours (green 255 read as the probe,
or black). covprobe now shades as refsw3 does (0 misses on all 96 sets); depthprobe compares in StoredDepth order.

**C3. Sliver colours**: the TSP colour model gives 0 at 870 sliver pixels a console shows (a determinant of a few
units: division by almost zero, 1/w below zero unmeasured); 4 sliver pixels with a negative model depth are drawn.

**C4. The regression's known misses** (2026-11-19, later the same day: detinf and modvol exact, README "refsw3 without float"; faredge 1330 / 1340 left), hugepos from 2^114 and the
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

**C9. HALF_OFFSET**: done 2026-11-19 (README halfofs2): refsw3 is the console at all 8 values, SupSample included.

**C10. SupSample with the rest**: punch-through in a modifier volume (the PT_MOD pass), translucent modifier
volumes, sprites, 16 bit UVs, palette and YUV textures, render to texture, DCalcCtrl with perspective.

**C11. Dumps not run**: the 4 that keep a console busy for minutes (garou_intro, garou_title, gunbird2_title,
td6_menu: what keeps it busy, see T below) and the 65 old-format dumps.

**C12. refsw2** (the emulator's renderer): its SupSample, trilinear and bump-light ports are unverified; its mipmap
level is a rough estimate (no console test is close); no strip buffer. Score it against the archived tests once
its mipmap level follows refsw3's.

**C13. The core's persistent offset colour (found by purist)**: done 2026-11-19 (README offspersist): the last
vertex's offset colour, as sent, of the last primitive with the offset bit the TSP shaded; across tiles, renders and
core resets; 0 after power-on; only bump maps without the offset bit read it. Modelled (offsKept). Left: an all-zero
setter (dropped planes), how long it lasts powered off.

**C14. The number formats at their edges** (README "refsw3 without float", 2026-11-19): cullval (register size
only, NaN = +inf), perpeq, zneg, trimin, bgspec (with zsign, bgneg, bgneg2, bgbig: the ISP's integer depth order,
the negative background, saturation at 2^127), zspec (with zbad, zbadbg, zbadk, zovf, zword: the overflow
regimes, the wrapping register and its words, underflow of slopes and start; detinf exact with it), modvol (the
edge start of +-2^132), nantr (the overflowing register's start and its words between steps), texfar (with
texfar_*: coordinates never overflow, the declared-short mip V, the -1 unit 1/w) done and modelled. Open:
faredge's last 10 regions (page 4's strip, page 1's control strip: when the edge unit keeps 2^127 instead of
2^132), zovf's exactly-2^128 cells.

## D. Timing probes

- **D1. SupSample's cost**: per pixel and per run against the same scene without it (four points: four times the
  TSP's work, or a pipeline that hides some), with point, bilinear, mipmapped, fog, offset.
- **D2.** Trilinear (two passes: twice the scene), two-volume polygons inside and outside a volume, bump maps
  (bumphang: they stop the render sooner), the punch-through passes.
- **D3.** The span sorter settings (A1: done for bits 0 and 8, README spansortcfg) and the TSP cache bypass, SPAN_SORT_CFG bit 16 and the per-polygon
  CacheBypass bit: parameter fetch cost per primitive with and without the cache.
- **D4.** FPU_PERP_VAL (A2): the ISP's cost per edge-on triangle.
- **D5.** The YUV converter's throughput (B).
- **D6.** The busy dumps (C11): replay each on a console (pvr-renderer) and time STARTRENDER to the end of render;
  which tiles take the time (one tile at a time in the region array).
- **D7.** SDRAM and burst settings (A5).
- **D8.** Chunked punch-through and the translucent sort cache against the ISP out of cache error (A4); and when a
  chunk's resolved pixels carry into the next chunk's runs (ptchunkcarry_races: by where the ISP is against the TSP).
