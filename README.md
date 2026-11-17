# pvr_tests

Tests that run the same KallistiOS program on a real Dreamcast (through dc-load) and in nulldc3's refsw2,
and compare the framebuffers.

Each test draws flat, known inputs, then reads the displayed framebuffer back (640x480, 32 bits per pixel,
read from video memory 32 bits at a time) and writes it to the host. The emulator runs the same program
headless and saves a frame as PNG.

Newer tests (covmeas, fbpack) do not use KallistiOS's PVR driver: they write the registers themselves, and the
same source also feeds nulldc3's refsw2-offline on a PC, which takes under a second. See "Direct harness and
refsw2-offline" near the end.

## blend

Every blend setting of the translucent blend unit: 8 source factors x 8 destination factors x SrcSel x
DstSel = 256 settings, each with 16 input color combinations (4 color sets x 4 squares), and four views
(framebuffer color, buffer 2 color, framebuffer alpha, buffer 2 alpha).

    cd blend
    bash run.sh 192.168.178.107            # hardware + emulator; "none" skips the hardware
    python compare.py /tmp/pvr_tests/blend # per setting: do hardware and emulator agree?
    python fit.py /tmp/pvr_tests/blend     # which arithmetic reproduces each of them?

`compare.py` and `fit.py` need numpy and Pillow.

The test uses KOS's `PM_RGB0888` framebuffer. Stock refsw2 asserts on that mode; the emulator build used
for the results below had its framebuffer write patched for it (and nothing else that affects blending).

### Results (2026-11-15, one console)

Color results of the 192 settings that do not have both SrcSel and DstSel set, 3072 squares:

| formula | hardware | refsw2 |
|---|---|---|
| refsw2's: `(src*ks + dst*kd) >> 8`, `k = c + (c >> 7)`, `c = x` or `255 - x` | 1136 | 3072 (all) |
| `(src*ks >> 8) + (dst*kd >> 8)`, `k = x'` or `256 - x'`, `x' = x`, except 255 -> 256 | 3072 (all) | 1136 |

So on hardware:

- a coefficient value x of 255 counts as 256, every other value counts as itself (128 is 128, not 129);
- an inverted coefficient is `256 - x'`, so a factor and its inverse always add up to exactly 256;
- each of the two products is shifted down by 8 separately (truncated) and then they are added and
  saturated. refsw2 adds first and shifts once.

The differences are 1 to 3 levels per channel, in every setting that has a multiply in it. Settings made of
ZERO and ONE only agree exactly.

Buffer selection (source = SrcSel ? buffer 2 : polygon, destination = DstSel ? buffer 2 : framebuffer) is the
same on hardware and in refsw2 for the three combinations above.

**SrcSel and DstSel both set** (read and write buffer 2 in one polygon): the polygon writes nothing, buffer 2
stays exactly as it was, with every pair of factors (refsw2 does the same). An earlier version of this
section had 35 of 1024 squares changing to values that are not the blend result; that was the test, not
the blend unit: those cells were the last ones on the screen, and the empty tiles after them damage the
tile before. See "blendsel, blendsel2, blendpos" below, also for the one thing such a polygon does do:
after ONE or SRC_A with ZERO, the alpha of the polygons that follow in the tile gets stuck.

Not analyzed yet: the alpha channel (views 2 and 3 are captured and compared, but `fit.py` only models
color; reading alpha goes through one more blend, so the raw comparison shows the rounding difference twice).

## The other tests

`common/pvrtest.h` is a small harness: the screen is a grid of 32x32 cells, a test draws a known thing in
each and labels it. `run.sh` builds and runs a test on both sides, `compare.py` reports per labeled region.

    bash run.sh texfmt 192.168.178.107        # texfmt, sample, shade, depth, raster
    python compare.py texfmt [-v | -q]        # also writes page<n>.cmp.png: hardware | emulator | difference

Results of 2026-11-15 (one console, refsw2 with only the 0888 framebuffer write patched). "Regions" are
labeled cells or spans; a region counts as matching only if every pixel is identical.

| test | what | regions matching |
|---|---|---|
| texfmt | texel formats and palette formats | 265 of 540 |
| sample | sampling position, bilinear weights, wrap modes | 10 of 250 |
| shade | Gouraud, texture environment | 40 of 149 |
| depth | compare modes, depth write, precision | 265 of 336 |
| raster | coverage, shared edges, culling | 1003 of 1040 |

### texfmt

- RGB565, ARGB1555, ARGB4444, as textures and as palette entries: hardware expands each channel to 8 bits by
  **bit replication** (5 bits: `v << 3 | v >> 2`, 6 bits: `v << 2 | v >> 4`, 4 bits: `v * 17`), so full
  scale is 255. refsw2 shifts, so full scale is 248 / 252 / 240. The values agree only near zero.
- ARGB8888 palette entries agree.
- YUV422: Y alone agrees over the whole range. With chroma, hardware and refsw2 differ by 1 or 2 in the
  channels that get a chroma term (rounding of the U and V products).

### sample

- **Point sampling**: hardware samples at the pixel center (a hair below it: a coordinate exactly on a texel
  boundary resolves to the lower texel). refsw2 samples at the pixel center minus half a *texel*, so a
  magnified, point-sampled texture is shifted by half a texel (2 pixels at 4x) and a 1:1 texture by half a
  pixel.
- **Bilinear**: for a 0 -> 255 step stretched over 32 pixels hardware gives 0, 7, 15, 23 .. (= 255 * n / 32,
  truncated) and the values do not move when the texture is shifted by up to 4/256 of a texel; refsw2 gives
  4, 12, 20 .. and follows every 1/256. So hardware's weight has a coarser fraction than refsw2's here, and
  its sample point is again just below the pixel center where refsw2's is the center. Not yet separated:
  whether the coarseness is in the weight or in the coordinate.
- Wrap modes: every cell differs, but through the sampling offset above; the wrap logic itself has not
  been compared independently yet.

### shade

- Gouraud: the same within 1 level; hardware is the lower one where they differ (for example the middle of
  a 0 -> 255 ramp is 127 on hardware, 128 in refsw2; the end of a 256 pixel ramp is 254 against 255).
- Texture environment: all 64 cells differ, mostly because of the ARGB4444 expansion above (255 against
  240). The combine logic has not been separated from that yet.

### depth

- Unequal depths: every compare mode in every list agrees.
- **Equal depths** (B drawn at exactly the depth already stored): on hardware, in the opaque and translucent
  lists, the new polygon behaves as *less*: `less` and `notequal` pass, `equal` and `gequal` fail.
  refsw2 treats them as equal.
- **Punch-through list at equal depth**: on hardware the second polygon never replaces the first, whatever
  the compare mode (even `always`); in refsw2 it does.
- **Precision**: with `greater`, hardware resolves a relative depth difference down to about 2^-17 .. 2^-20
  (2^-17 at z = 1, 2^-18 or 2^-19 at z = 1000, 2^-17 or 2^-20 at z = 0.001, depending on the list); refsw2
  resolves 2^-23 or 2^-24, i.e. full single precision.

### raster

- Coverage of axis-aligned quads and of triangles at all 1/16 pixel offsets: identical.
- Two triangles sharing an edge, as separate triangles and as a strip, 240 random shapes each: identical
  (the same pixels covered once, twice and not at all on both sides).
- **Counter-clockwise triangles whose edges pass exactly through pixel centers**: hardware draws the pixels
  on the top and left edges, refsw2 leaves them out (46 pixels of a 16 pixel triangle).
- **Small polygons**: with culling `small`, `ccw` or `cw`, hardware drops triangles with legs of 1 pixel and
  below; refsw2 draws them. With culling off both draw them, except the very smallest (1/32 pixel), which
  only refsw2 draws.

## Second batch (2026-11-15)

    bash run.sh <test> 192.168.178.107     # env alphatest fog bump mip modvol sort sprite fb565 fb555 scaler

Tests that need point-sampled, non-uniform textures add `POINT_FIX` (half a texel, emulator builds only) to
their coordinates, so that the sampling difference found by `sample` does not hide what they test. Exact
texel colors come from 8 bpp textures with an ARGB8888 palette, which both sides decode identically.

| test | what | regions matching |
|---|---|---|
| env | texture environment, wrap modes, color clamp | 127 of 208 |
| alphatest | punch-through alpha test | 640 of 960 |
| fog | table fog, vertex fog, table mode 2 | 532 of 640 |
| bump | bump map intensity | 320 of 640 |
| mip | mipmap level selection, trilinear, VQ, stride | 0 of 543 (colors differ by the texel expansion; see below) |
| modvol | modifier volumes (opaque) | see below |
| sort | translucent autosort | 28 of 56 |
| sprite | sprites | 14 of 64 |
| fb565, fb555 | 16 bit framebuffers, dither | 40 and 76 of 516 |
| scaler | SCALER_CTL | inconclusive |

### env

- Texture environment: the combine logic is the same for all 32 combinations in both lists, in color and
  in alpha. The remaining differences are 1 level, in the multiplies: hardware computes modulate as
  `(a * b) >> 8` with nothing added, so 255 x 255 gives 254 and 192 x 255 gives 191; refsw2 gives 255 and 192.
  (This is not the blend unit's rule, where a coefficient of 255 counts as 256.)
- Wrap modes: all 16 combinations of flip and clamp are identical.
- Color clamp: identical, apart from the same modulate rounding.

### alphatest

- The pass/fail threshold is identical for every alpha 0..255 at reference values 0, 1, 127, 128 and 255,
  with the alpha coming from a texture or from the vertex.
- The only difference is again the modulate rounding (texel 255 x vertex color 255 = 254 on hardware).

### fog

- Vertex fog, table fog mode 2 and no fog: identical.
- Table fog (linear and a sawtooth table): the same within 1 or 2 levels in about half of the squares.
- KOS's `pvr_fog_vertex_color()` asserts "not implemented"; the test writes the register itself.

### bump

- With the replace environment, the bump intensity agrees for all 256 (S, R) combinations and all five
  sets of light parameters.
- With the modulate environment, hardware multiplies the base color by the intensity (base 0xc0ff40 at
  full intensity gives 191, 254, 63); refsw2 outputs the intensity alone (255, 255, 255). All 64 cells of
  every page differ.

### mip

Level shown (0 = 16x16 .. 3 = 2x2) for a square quad of 32, 24, 16, 12, 8, 6, 4, 3, 2, 1 pixels, texture 16x16:

    bias 1.00, point      hardware 0 0 0 0 0 1 1 2 2 3      refsw2 - - 0 0 1 1 2 2 3 3
    bias 0.25, point      hardware 0 0 0 0 0 0 0 0 0 1      refsw2 - - 0 0 0 0 0 0 1 2
    bias 3.75, point      hardware 0 1 1 2 2 3 3 3 3 3      refsw2 0 0 1 1 2 2 3 3 3 3

- **Level selection differs.** At bias 1.0 refsw2 switches to the next level at exactly 2:1 minification
  (8 pixels); hardware is still on level 0 there and switches later. At small biases hardware stays on the
  large level much longer; at large biases hardware leaves it earlier.
- **Magnified, mipmapped, point sampled:** refsw2 draws black (`-`) for the 32 and 24 pixel quads at most
  biases; hardware draws level 0.
- **Non-square quads:** hardware picks the level from the more compressed axis (32x4 pixels at bias 1.0
  gives level 1, 16x2 level 2); refsw2 from the less compressed one (level 0 for both).
- **Trilinear (both passes): refsw2 asserts** in `TextureFilter` (refsw_tile.cpp:1035). Hardware results
  are in the dumps: pass 1 fades each level out toward the next (for example 254, 171, 3 of red at 16, 12,
  8 pixels), pass 2 fades the next one in (83, 251 of green at 12, 8 pixels).
- VQ (codebook and index order) and strided textures: same layout on both sides; the colors differ only by
  the texel expansion.

### modvol

Opaque list, cheap shadow: which pixels are inside is identical for every arrangement (one sheet in front,
behind, two, three, front plus behind, two volumes, exclusion volume). Differences:

- Intensity 0x80: hardware gives `(c * 0x80) >> 8` (0x4080c0 becomes 0x204060); refsw2 0x204161.
- Intensity 0xff: hardware darkens by one level (0x3f7fbf); refsw2 leaves the color unchanged.
- With intensity mode on (bit 8 of the register), two-volume polygons come out black on both sides; with
  it off, two-volume polygons work and cheap-shadow polygons come out black, on both sides.

**Not resolved:** submitting the translucent modifier list (`-DWITH_TR_MOD`) gives a completely black frame
on hardware, while refsw2 draws it. This may be how the test drives KOS rather than the hardware, so the
default build leaves that list out and translucent modifier volumes are untested.

(2026-11-16: tested in timemeas with the framebuffer read back, see timemeas/REPORT.md. They work in a tile
sorted per pixel and are ignored in a pre-sorted one; this test renders pre-sorted.)

### sort

Autosort agrees in every case: the six depth orders, ties, intersecting polygons, depth write off, compare
always, with and without an opaque polygon in between. Additive blending is identical; alpha blending
differs only by the blend unit's rounding (1 level).

### sprite

- Textured sprites that both sides draw have the same geometry and texel mapping (flips, swapped axes,
  repeats); colors differ by the modulate rounding (35 against 36).
- **refsw2 does not draw some sprites that hardware draws:** untextured 16x16 sprites at (8 + i/16, 8 + i/16)
  (all 16 offsets), the same placement textured (all 16), and one of the eight corner orders of a 20x16
  rectangle (cells 40 and 47 of page 0).
- Sprites whose fourth corner is not where the first three imply (and the four parallelograms) differ in
  hundreds of pixels: hardware draws more than refsw2.
- Sloped depth across a sprite: identical in 7 of 8 cells.

### fb565 / fb555

- **Without dither** hardware truncates: 8 bit v becomes `v >> 3` (and `v >> 2` for 565 green). The refsw2
  frame is one step lower at every multiple of 8 (8 -> 0, 16 -> 1, 128 -> 15, 248 -> 30), as if it packed
  `v - 1`.
- **With dither** both dither, with different patterns and thresholds. Hardware, red, input 1 / 3 / 4 / 7:
  2, 6, 8 and 14 of 16 pixels raised; values that are exact multiples of 8 stay undithered on hardware
  but not in refsw2.

### scaler

Superseded: see "scalermeas, scalertile, sof2probe" below. This test changed SCALER_CTL between frames, which
stalls the core on a console, and drew about 21,000 quads a page, more than the parameter memory holds.

### blend, alpha channel

`python blend/alpha.py /tmp/pvr_tests/blend`: the alpha that the blend unit writes follows the same
arithmetic as the color channels, for the framebuffer and for buffer 2: 3072 of 3072 squares each.

## precision: depth buffer and triangle setup (2026-11-15)

    bash run.sh precision 192.168.178.107
    python precision/precision.py /tmp/pvr_tests/precision

Depths are given as float bit patterns; "unit" below is one unit in the last place of a 32 bit float.
refsw2 behaves as exact single precision in every part of this test (every 1 unit step resolved, equality
only at 0 units, edges resolved down to what a float can hold).

**The depth buffer itself keeps what setup computed.** A second quad with exactly the first one's vertices
and depth compares EQUAL on every pixel, for all 14 depths tried, and a 1 unit change of the input can flip
the outcome. So the loss of precision is not in storing or comparing.

**Triangle setup does not reproduce the vertex depth of a flat polygon exactly.**

- The outcome is uniform inside a triangle and often differs between the two triangles of one quad
  (half-cell patterns along the diagonal): each triangle gets its own plane, a few units apart.
- A flat quad drawn again with different vertices or shifted by a few units compares equal, less or greater
  depending on the triangle; EQUAL passes at scattered offsets within about +-10 units, not at a contiguous
  range and often not at 0. This is why "equal depth" looked like "less" in the `depth` test.
- A difference is resolved reliably only from 64 units upward (2^-17 relative) and from 32 units downward.
  Depths whose low mantissa bits are zero need +64 but only -1; depths whose low bits are all ones need +4
  but -32. That asymmetry is what truncating about 6 low bits of the mantissa (to roughly 18 bits) would do;
  the computed depth is biased low.
- The same flat square (z = 1.0) through its two triangulations, all over the screen: they differ by 16 to
  64 units. It needs +32 units to be resolved everywhere for 32x32 squares and +64 for 8x8 squares in the
  right part of the screen; the pattern changes at x = 256 and at y = 256 (where the coordinates' float
  exponent changes) and small polygons are worse. So the error also grows with coordinate / polygon size,
  as arithmetic on absolute screen coordinates in limited precision would.
- A sloped plane against a flat one at its middle value: the crossing is at the exact pixel as long as the
  plane rises by at least z * 2^-17 over 32 pixels, for z = 1, 1.5, 1000, 0.001, in x and in y, at every
  screen column. With smaller slopes the flat polygon loses everywhere or the result is irregular.

**Edges.** For an edge 2^-n pixels past a pixel center (the pixel should not be drawn), hardware stops
resolving the offset at n = 12 or 13 for a left edge near x = 16, at n = 8 to 12 further right or with the
other winding, and at n = 6 to 14 for a top edge depending on the column; below that the pixel is drawn.
An edge just before the center always draws the pixel. Offset plus coordinate magnitude comes to about
21 to 25 bits, i.e. edge functions evaluated in single precision on absolute coordinates.

**Summary.** Hardware behaves like single precision arithmetic with roughly 18 good mantissa bits in the
depth plane (2^-17 relative, biased low) and edge positions good to 2^-8 .. 2^-13 pixel depending on where
the polygon is. The exact internal format is not pinned down: these are bounds from outcomes, not a model
that reproduces every cell.

## huge: very large, very small and special values (2026-11-15)

    bash run.sh huge 192.168.178.107
    python huge/huge.py /tmp/pvr_tests/huge

**Vertex positions.** One triangle with corners (-4M, -3M), (4M, 3M), (-3M, 4M), which should cover exactly
the 153600 pixels below the screen diagonal:

| M | hardware | refsw2 |
|---|---|---|
| 2^8 .. 2^24 | exact | exact |
| 2^30 | exact | 56402 pixels wrong |
| 2^40, 2^60 | exact | 147620 pixels wrong (most of the screen lit) |
| 2^100 | exact | nothing drawn |
| 2^120 | 705 pixels drawn | nothing drawn |
| infinity, or NaN in one coordinate | nothing drawn | nothing drawn |

Hardware places this edge on the exact pixels up to coordinates of 2^102; refsw2 is only right up to
about 2^24. (The corners here are exact powers of two and the edge passes through the origin, so this says
the hardware's range is very large, not that arbitrary huge coordinates are placed exactly.)

**Depth magnitude.** For z = 2^-113 .. 2^121 (every 13th exponent) both sides agree and behave normally:
2z beats z, z + 64 units beats z, z / 2 does not. Differences:

- **Negative depth:** hardware does not draw a polygon with negative z at all, even with compare ALWAYS;
  refsw2 draws it. In comparisons both treat it as behind everything.
  (2026-11-17: it is drawn, and black. See "The TSP's numbers at their limits".)
- **z = 2^127:** hardware draws it, refsw2 does not.
- **Special values** (0, -0, denormals, smallest normal, largest finite, infinities, NaNs) with compare
  ALWAYS: hardware draws all of them; refsw2 draws only the denormals and the smallest normal.
  With the largest finite value, +infinity and NaN, hardware's output is irregular: often only one of the
  quad's two triangles appears.
- At the extremes (2^-126, 2^127, their negatives, and the special values) hardware also produces stray
  white areas that reach into neighbouring cells (visible in page0.cmp.png at columns 0 and 19). I have not
  worked out which polygon they come from, so the first and last columns of the comparison rows on that
  page are not usable.

## refsw2 after the fixes (2026-11-15, uncommitted working tree of nulldc3)

`bash score.sh <emulator build dir>` re-runs every test in the emulator only and leaves the frames next to
the hardware dumps; then run `compare.py <test> -q` for each. Regions that match the console exactly:

| test | before | after | what is left |
|---|---|---|---|
| texfmt | 265 of 540 | 540 of 540 | |
| sample | 10 of 250 | 250 of 250 | |
| env | 127 of 208 | 208 of 208 | |
| alphatest | 640 of 960 | 960 of 960 | |
| bump | 320 of 640 | 640 of 640 | |
| sort | 28 of 56 | 56 of 56 | |
| depth | 265 of 336 | 336 of 336 | |
| raster | 1003 of 1040 | 1040 of 1040 | |
| precision | 2967 of 6180 | 6179 of 6180 | one edge-position cell |
| zblock | - | 1320 of 1440 | color of a steeply sloped Gouraud quad, 1 level (all depth outcomes match) |
| fb565 / fb555 | 40 / 76 of 516 | 515 / 515 of 516 | the Gouraud ramp row, 1 level in 24 pixels |
| fog | 532 of 640 | 636 of 640 | 4 squares, 1 level |
| shade | 40 of 149 | 148 of 149 | 63 pixels of the 640 pixel Gouraud ramp, 1 level |
| sprite | 14 of 64 | 64 of 64 | |
| mip | 0 of 543 | 1019 of 1083 | trilinear on quads whose size is not a power of two, up to 4 levels |
| blend (settings) | 0 of 256 | 238 of 256 | buffer 2 -> buffer 2 settings where hardware writes stale data |
| huge | - | 196 of 250 | cells the console covers with stray geometry; irregular output at infinity / NaN |
| modvol | - | 171 of 192 | polygons whose vertex type does not match the shadow mode |

### The setup unit's arithmetic

Fitted offline against the console's depth outcomes: 20880 of 20880 comparisons between flat polygons with
different vertices, sizes, positions and depths (`precision` pages 0-2 and 6-21), and all 22500 with
sloped planes. Now in refsw2 as `DepthPlane`:

- every operation keeps a 24 bit mantissa and truncates toward zero;
- the plane is built from edge constants on absolute screen coordinates:
  `e1 = x2 y3 - x3 y2`, `e2 = x3 y1 - x1 y3`, `e3 = x1 y2 - x2 y1`, `det = (e1 + e2) + e3`,
  `a = (z1 (y2 - y3) + z2 (y3 - y1)) + z3 (y1 - y2)`, `b` likewise with x, `c = (z1 e1 + z2 e2) + z3 e3`;
- `a`, `b`, `c` are each multiplied by the truncated reciprocal of `det`;
- the vertices are taken as (v1, v3, v2) of the order the rasterizer gets them in (odd strip triangles
  have their first two swapped);
- within a tile the depth is interpolated in block floating point, on the exponent of the depth at the
  tile's first pixel center: that start value is truncated to 20 bits below the exponent; the slopes A and B
  are truncated to a 20 bit mantissa of their own and to 26 bits below the same exponent; a pixel's depth is
  the start plus whole pixel steps of those;
- what is compared and stored is that value normalized again, truncated to a 19 bit mantissa on its own
  exponent (not the tile's).

With this the fit is 59220 of 59220 over `precision` and `zblock` together, gentle and steep slopes included
(`fit/zslope.py`). A per-pixel float depth cut to a 19 bit mantissa gets 43275 of the 43380 `precision`
observations; keeping the stored value on the tile's exponent fits `precision` equally well but is ruled out
by `zblock`.

### zblock

`bash run.sh zblock <ip>`, then `python zblock/zblock.py`. A quad whose depth falls from 32 to 0.52 (or rises
from 1 to 32.77) across one tile, probed in its last pixel columns by a flat quad in fine depth steps.

- Falling: near depth 1 the console still tells steps of 2^-18 apart (B passes GREATER from j = 5, EQUAL at
  j = 3..4). A value kept on the grid of the tile's start exponent (32) could only resolve 2^-13.
- Rising: near depth 32 the console resolves 2^-14 steps in pairs (EQUAL at j = 0..1), i.e. a 2^-13 grid,
  the 19 bit mantissa of a value near 32. On the start exponent's grid (1) it would resolve 2^-18.

So the interpolation is on a shared exponent and the result is stored as a float with a 19 bit mantissa.
refsw2 matches every depth outcome of this test; the cells that still differ (120 of 1440) differ in color:
the flat red, Gouraud shaded, steeply sloped quad comes out as 254 instead of 255 in a band of about 62
pixels in some cells on the console, depending on where on the screen the cell is.

The sensitivity of the fit: 23 or 26 bits instead of 24 drops it to 90 % and 87 %; rounding to nearest to
75 %; coordinates relative to the tile to 84 %; an exact division instead of the reciprocal to 99 %; a
stored mantissa of 18 or 20 bits to 93 % and 94 %. The same arithmetic for the edge functions reproduces
how finely an edge is resolved (39 of 40 cells of page 5) and coverage (`raster` 1040 of 1040).

Other things fitted from the dumps: bilinear is two truncated steps (along u, then between rows) with 5 bit
weights (52224 of 52224 pixels); interpolated colors round up from a fraction of about 0.497 (all but 63 of
81408 pixels of the Gouraud page); the mipmap level
and trilinear fraction come from a value in 1/64 steps; the YUV decode is shift-and-add.

### What changed in refsw2

Blend unit arithmetic and the buffer 2 -> buffer 2 case; texel expansion by bit replication; YUV decode;
modulate / decal / modulate-alpha rounding; point sampling at the pixel center with boundary ties going to
the lower texel; bilinear; mipmap level selection; trilinear; bump result through the texture environment;
cheap shadow multiply; fog weights; 16 bit framebuffer truncation and the measured dither matrix; top-left
rule for counter-clockwise triangles; a wrong edge in the sprite row test; depth plane and edge functions in
the setup unit's arithmetic; negative depth not drawn; small-polygon culling including the threshold itself;
FPU_CULL_VAL reset value 1.0 (what a console has when a program starts); interpolation in double.

### ztile: the shared exponent is per polygon, per tile

`bash run.sh ztile <ip>`, then `python fit/ztilefit.py`. The exponent is each polygon's own (a steep quad
starting at 32 and a flat probe at 1 in the same tile each use theirs). Whether it is derived again in every
tile is what this test separates: a sloped quad that covers only the right half of a tile (the plane extended
back to the tile's first pixel is in a higher binade), and one that spans two tiles.

| reading of the model | observations reproduced |
|---|---|
| per tile: start at the first pixel of the tile being drawn, even if the polygon does not cover it | 11280 of 11280 |
| per triangle: start at the polygon's own top left pixel | 11266 |
| per triangle: start at the first pixel of the first tile the polygon touches | 11268 |

So the start value and its exponent are taken again at the first pixel center of every tile. The margin is
a dozen observations, all in the two-tile and half-tile cases, which is where the readings differ.
refsw2 (which sets the plane up per tile) matches every depth outcome of this test; what differs there is
again the color of the steep Gouraud quad, by 1 or 2 levels.

### halfofs: the pixel / texel center register

`bash run.sh halfofs <ip>`. The same picture with HALF_OFFSET (0x80) set to each of 0..7: bit 0 is the ISP
pixel half offset, bit 1 the TSP's, bit 2 the texel's. The picture has coverage at 1/8 pixel offsets, sloped
depth against a flat probe, Gouraud ramps, point and bilinear sampling with shifted coordinates, and
perspective.

refsw2 matches the console in all eight settings: 1080 of 1080 regions.

What each bit changes on the console (pixels that differ from the setting 7, by area):

| area | ISP bit clear | TSP bit clear | texel bit clear |
|---|---|---|---|
| coverage | 992 | 0 | 0 |
| sloped depth against a flat probe | 0 | 0 | 0 |
| Gouraud ramps | 0 | 3072 | 0 |
| point sampling (4x and 1:1) | 0 | 5632 and 536 | 0 |
| bilinear | 0 | changes | changes |
| perspective texturing | 0 | changes | changes (bilinear cells) |

So the ISP bit moves where coverage is sampled; the TSP bit moves where colors and texture coordinates are
evaluated; the texel bit only matters for bilinear filtering. With both the TSP and texel bits set, clearing
the ISP bit changes nothing but coverage.

**The TSP evaluates 1/w itself.** refsw2 computes 1/w for perspective in the TSP, at the TSP's sample
position. As an experiment I made it take 1/w at the ISP's sample position instead: the four settings where
the two bits are equal still matched, and the four where they differ (1, 2, 5, 6) each lost 6 or 8 regions,
all of them the perspective cells (Gouraud with sloped depth by up to 17 levels, perspective texturing by
whole texels). So on the console the depth used for perspective follows the TSP bit, not the ISP bit: it is
calculated again in the TSP and not taken from the ISP's depth buffer.

### tspw: the TSP's own 1/w, and the TSP plane model

`bash run.sh tspw <ip>`. Table fog with every table pair set to (0, 255), black polygons and white fog: a
pixel's gray level is then 8 bits of the mantissa of the TSP's 1/w (bits 11 to 18), so 12 bits of it are
visible directly.

For flat and gently rising depth the TSP's 1/w agrees with the ISP model. For a plane falling from 32 to
0.52 across a tile it is low by an amount that is the same in every row and every pixel of a cell and depends
only on the cell's screen column: 2 steps at x = 32, 4 at 64, 8 at 128, 16 at 256, 30 at 288, 12 at 320,
36 at 512. That is exactly what is lost by cutting the plane's constant term (its value at the screen's
origin, 32 + 0.98 x0 here) to a 16 bit mantissa.

The TSP plane model, now in refsw2 (`PlaneStepper3`):

- set up like the ISP's depth plane, on absolute screen coordinates;
- the three coefficients are then held as floats with a 16 bit mantissa;
- a pixel's value is `(A x + B y) + C` on absolute pixel coordinates in 20 bit arithmetic, with no per-tile
  block exponent;
- an attribute is (attribute * 1/w plane) / (1/w plane), both planes of this kind.

It reproduces 98 % of 23840 fog readings exactly (99.5 % within one step), 564 of the 666 console 254s of
the steep Gouraud quads with 24 false ones, and all sampled pixels of the Gouraud page. In refsw2 it changed
no test that already matched and brought fog to 640 of 640, tspw from 1242 to 1563 of 1920, zblock to 1344
of 1440 and ztile to 780 of 960.

Whether the division by 1/w is exact or uses an approximate reciprocal cannot be told from these tests: with
a flat color the ratio is always close to 1 and every variant scores the same.

### ops: one operation at a time

    python ops/gen.py                  # makes ops/cases.h and ops/cases.json
    bash run.sh ops <ip>
    python ops/analyze.py /tmp/pvr_tests/ops

`ops/model.py` is the fitted arithmetic with every multiply, add, subtract, reciprocal, truncation and grid
step as a named operation with its own width and rounding. `gen.py` searches random triangles (any shape,
half of them with arbitrary float coordinates, anywhere on the screen) for inputs on which changing ONE
operation (one or two bits more or less, rounding to nearest, down or up, a reciprocal instead of a division)
changes what the console would show, and keeps up to 10 per alternative. Four observables: ISP depth (a
compare EQUAL against a flat quad whose depth the model predicts to be equal), the TSP's 1/w (fog readout), a
Gouraud gray, and coverage of a pixel next to an edge. `analyze.py` prints, per alternative, on how many of
its cases the console sides with the model, with the alternative, or with neither.

First run (487 cases, 2026-11-15):

| observable | model agrees with the console | refsw2 agrees with the model |
|---|---|---|
| ISP depth, equal to the predicted value | 71 of 167 | 167 of 167 |
| TSP 1/w, fog readout | 107 of 169 exactly, 154 within one step | 169 of 169 |
| Gouraud gray | 63 of 95 exactly, all 95 within one level | 83 of 95 |
| coverage | 54 of 56 | 56 of 56 |

What this says:

- **The ISP depth model is not right for general triangles.** It was fitted on the two triangles of
  axis-aligned quads with integer corners, where it reproduces everything; on arbitrary triangles the console
  lands on the predicted 19 bit value in only 71 of 167 cases. Because of that the per-operation table for
  the setup and ISP operations mostly shows "the console disagrees with the model" in every row and cannot
  yet be read as a verdict on single operations.
- **TSP coefficients:** 16 bits wins against 14, 15, 17 and 18 for both the slopes and the constant.
- **TSP per-pixel arithmetic:** 21 or 22 bits fit better than the 20 in the model (the console sides with
  22 bits on 30 to 33 of about 45 cases and with 20 on 2 to 10); 18 and 19 are worse.
- **Division by 1/w, attribute premultiply, the three per-pixel edge operations:** no or almost no input was
  found that separates the alternatives, so these are not characterized. They need targeted inputs.

That measurement is `zmeas`, below; it settled the ISP depth.

**Second run, with the final model (2026-11-16).** After zmeas, wmeas, uvmeas, cmeas, uvspan and covmeas the
model was rewritten operation by operation (new operations: the sum orders, the ISP start and slope grids,
the TSP line grid and run start, the reciprocal table, the edge start, slopes and zero zone), the cases
generated again (526; `gen.py` now takes about 9 minutes) and run:

| observable | console = model |
|---|---|
| ISP depth, equal to the predicted value | 197 of 197 |
| TSP 1/w, fog readout | 161 of 161 |
| Gouraud gray | 107 of 107 |
| coverage | 61 of 61 |

Every case is one on which some single change of one operation would show something else, so this is the
per-operation verdict the first run could not give. `analyze.py`: of 219 alternatives (a bit or two more or
less, another rounding direction, another sum order, for each of 37 operations) 167 have cases that tell them
from the model, and the console sides with the model on every case of every one of them; none gets a single
case. For the other 52 the generator found no input among 60,000 random triangles: the widths of the edge
evaluation (those are what covmeas measures), a few bits more in a product whose low bits are cut by the next
step anyway, and rounding directions that coincide with truncation on the inputs tried. (The cases of the
first run, predicted again with the final model: 167, 169, 95 and 56, all of them.)

## zmeas: the ISP depth of general triangles, measured as a number (2026-11-15)

    python zmeas/gen.py --mix first|general|zero|scene [--seed N] [--refine meas.json]
    bash run.sh zmeas <ip> [emulator dir]
    python zmeas/read.py /tmp/pvr_tests/zmeas [hw|emu]        # -> meas.json: the stored depth per pixel

Every page draws the polygons under test (depth always passes and is written) and then, over each 32x32
cell, a flat quad with compare GREATER whose depth is the page's value of a 16 step ladder. The pages
bracket the stored depth between two stored depths of the quad; when those are neighbours on the 19 bit
grid the stored depth is known exactly. The stored depth of the flat quad comes from the model (that
family was already reproduced completely), and a calibration group checks it: flat full-cell quads with
15 bit depths read exactly their depth, 40 of 40. A run takes about 15 seconds and gives up to 7200
bounded pixels. The archived runs are in `fit/zmeas_data`.

(The first version of the probe was a huge flat triangle confined to the tile by the user tile clip. On a
console that does not work as documented, see `cliptest`.)

What the measurements changed in the model (`ops/model.py`, function `isp_depth`), in the order found:

1. **The order of the three-term sums.** In rasterizer vertex order the slopes and the determinant are
   `(t1 + t2) + t3` and the constant is `(t1 + t3) + t2`. Every add truncates to 24 bits, so the order is
   visible. This alone took the first set from 5417 to 7043 of 7091 pixels. Of the 384 combinations of
   vertex order and sum orders exactly one (and its mirror images) fits.
2. **The depth at the first pixel of the tile is not `A x0 + B y0 + C`.** It is the plane before its
   division by the determinant, evaluated at the first pixel center, times the reciprocal, once:
   `((a x0 + b y0) + c) * inv`, each step truncated to 24 bits. (14289 of 14291.)
3. **The two slopes share an exponent.** Each is `a * inv` or `b * inv` as a 20 bit float; in addition
   only 21 bits under the exponent of the larger of the two are kept; and both are cut to the grid of the
   tile, 26 bits under the exponent of the start value. (28686 of 28686.)

With those three, on sets measured after the model was fixed (so not fitted):

| set | pixels the model reproduces |
|---|---|
| general, seed 5 (small triangles; flat to depth over six octaves) | 7200 of 7200 |
| scene, seed 7 (triangles up to 1500 pixels off screen, drawn over each other) | 7200 of 7200 |
| zero, seed 6 (depth plane crosses zero next to the triangle) | 7193 of 7194 |
| all seven sets | 50279 of 50280 |

The one pixel left is a case where the start value is the difference of terms 2^8 times larger than itself
(a negative start, slopes 2^4 times the start per pixel); the console is one stored step above the model.
Probably guard bits in the adder that only show under that much cancellation. Not resolved.

How well the data pins each operation down: changing any single setup operation by one bit or to another
rounding direction loses between 30 and 1900 of the 14291 pixels of the first two sets, except the rounding
direction of the edge products and of the determinant sum for negative values (too few negative
coordinates), the width of `a * inv` beyond the 20 bits the slopes keep, and the width of the start beyond
its 20 bit grid.

The `ops` cases re-predicted with this model against the console dumps of the first run: ISP depth 167 of
167 (was 71), TSP 1/w 129 of 169 (was 107), Gouraud gray 66 of 95 (was 63), coverage 54 of 56.

refsw2 (`DepthPlane::Setup`) follows the model; on the zero set it gives the console value on 7195 of 7195
measured pixels. The older comparison tests are unchanged by it (precision 6179 of 6180, zblock 1344 of
1440, ztile 780 of 960, huge 195 of 250): their remaining differences are not depth.

## wmeas: the 1/w of the TSP at every pixel (2026-11-15)

    python wmeas/gen.py [--seed N] [--mix general|flatedge|flatscan|slopes]
    bash run.sh wmeas <ip> [emulator dir]
    python wmeas/read.py /tmp/pvr_tests/wmeas [hw|emu] [out.npz]

The fog readout of `tspw` (bits 11..18 of the mantissa of the 1/w the TSP computed) on general triangles;
every pixel at least 1.5 pixels inside a triangle is a reading, 400,000 to 780,000 per run. Archived runs
are in `fit/wmeas_data`. Two runs of the same 695,796 readings on the console are identical.

Established with it (the rest came from `uvmeas`, below, which reads plane values as numbers):

- Full-cell quad triangles with flat depth read exactly the 16 bit truncation of the constant of the 24
  bit setup, on 1600 of 1600 triangles, including depths walked through a step of the reading in units of
  the 24th bit, where the reading goes up and down several times.
- General triangles with flat depth read one level low in about 60 % of cases when the depth is less than
  one unit of the 16th bit above a step, and never from one unit up: the constant is cut to 16 bits and
  then tiny negative terms are rounded down. Which triangles depends on the sign of the slope terms (for
  a flat triangle the rounding noise of the setup) and on the winding; see point 5 below.

## uvmeas, cmeas: TSP plane values read through a wrapping texture coordinate, and colors (2026-11-15)

    python uvmeas/gen.py [--seed N] [--mix first|flatscan|steep|wflat|general|recip|recipscan|reciprow|reciprown]
    python cmeas/gen.py [--seed N]          # the same for Gouraud colors (run.sh cmeas, cmeas/read.py)
    bash run.sh uvmeas <ip> [emulator dir]
    python uvmeas/read.py /tmp/pvr_tests/uvmeas [hw|emu] [out.npz]

A 1024 texel wide texture in which texel i holds the number i, repeating, point sampled. With 1/w = 1 a
pixel shows floor(U * 1024) mod 1024: ten bits of the U the TSP computed there, and which ten depends on
the size of U. For U between 64 and 128 the lowest bit of the reading is half a unit of the 16th mantissa
bit, so the value the TSP delivers is read exactly, at every pixel: 780,000 numbers per run. Every polygon
is one triangle of the quad over a cell, so the plane is known exactly (A = (U2 - U1) / 32 and so on).
Archived runs: `fit/uvmeas_data`.

What it showed, in the order found:

1. **Coefficients are 16 bit floats.** A flat U with 24 significant bits reads as its 16 bit truncation, at
   every size from 1 to 2^22.
2. **An attribute arrives one unit of its 16th bit low.** That is the division by 1/w: it is a multiply by
   a reciprocal with a 16 bit mantissa in [1, 2), cut down, so the reciprocal of exactly 1 (or any power of
   two) is one unit under. This is the "a coordinate exactly on a texel boundary shows the texel before it"
   rule of the `sample` test; the special case in the texel address is gone from refsw2.
3. **The plane value is cut to 16 bits (down) before that multiply, and the product keeps 17 bits,
   rounded down** (toward minus infinity: a negative U arrives half a unit of the 16th bit above the
   plane, a positive one a whole unit below).
4. **Planes are evaluated per run of pixels, not per pixel.** The value at the first pixel of a run is
   `C + floor(A x0) + floor(B y)` and every further pixel adds `floor(A)`, all rounded down to one grid
   (which grid, and what a run is: see "uvspan" below; in these sets a run is a line of the cell and the
   grid is 22 bits under the exponent of C). A small negative slope therefore falls faster than the plane
   (1/64 of a unit per pixel at least), which is what the first runs showed as extra steps.
5. **The winding matters through the sign of zero.** A slope that cancels exactly (flat attribute) is a
   zero; times the reciprocal of a negative determinant it is a negative zero, and that is rounded down to
   minus one grid unit like any other negative number. So a flat plane on a triangle of one winding sits
   one grid unit lower per pixel than on the other. This is the 60 % of flat general triangles that read
   one fog level low in `wmeas`.
6. **The constant of an attribute plane is summed (t1 + t2) + t3**, not (t1 + t3) + t2 like the depth and
   the 1/w plane: 2400 of 2400 flat U values walked through 16 bit grid points in units of the 24th bit,
   against 2377 with the depth's order. The 1/w plane keeps the depth's order (fog readings of flat quads:
   1600 of 1600 against 1597).

7. **The division by 1/w is a multiply by a table reciprocal.** For a 16 bit divisor mantissa D (32768 to
   65535) the reciprocal is the 16 bit number `r = (64 t[i] - 1 - n[i] (D & 63)) >> 7` with `i = D >> 6`,
   `t[i] = ceil(2^32 / (64 i))` (the exact reciprocal at the start of the block, rounded up to half a unit)
   and a slope `n[i] = t[i] - t[i+1] + c[i]`, c[i] being 0, 1 or 2. It was read off the console for all
   32768 values of D (mix `recipscan`, U = 2^j over a depth of mantissa D shows D * r(D) directly; 14 runs);
   the 512 digits c[i] are in `ops/model.py` (`RECIP_C`) and in refsw2. No formula was found for them
   (102 zeros, 403 ones, 7 twos). r is the truncated exact reciprocal for 28487 values of D, one less for
   1110 and one more for 3171; 1 / 1.0 is 65535 / 65536.
8. **The product is cut before it is normalized.** numerator mantissa times reciprocal mantissa (both in
   [0.5, 1)) is rounded down, toward minus infinity, at 17 fractional bits; a product of at least 1/2 thus
   keeps 17 significant bits and a smaller one 16. With eight numerators per divisor (mixes `reciprow`,
   `reciprown`) this and the table reproduce 1,560,000 of 1,560,000 readings, positive and negative.
9. **An attribute is not multiplied by 1/w at the corner.** The attribute plane is made of the products of
   the 1/w plane, each multiplied by the attribute of its corner and cut to 24 bits again:
   `u_i * trunc(w_i * d)` for each of the nine products, not `(u_i * w_i) * d`. Flat U over a flat depth
   other than 1 (mix `wflat`): 600 of 600 against 591 for an exact product and 581 for a 24 bit one.

`cmeas` (Gouraud colors on the same kind of triangles, 780,000 pixels, three channels each) settles the
last step of a color: **the 8 bit color is floor(v + 0.5)** of the attribute. 2,340,000 of 2,340,000
readings, where 0.498 and 0.502 each lose about 0.4 %.

Model against console (`ops/model.py`: `tsp_plane`, `tsp_divide`, `tsp_attr`, `tsp_color`), and refsw2,
which follows it (`PlaneStepper3`, `TexCoordFixed`, `COLOR_BIAS`) and gives the same numbers:

With the two rules of the next section (where a run starts, and the grid of a tile) the model reproduces
every archived reading, and refsw2 gives the same numbers wherever it was re-run (all but the reciprocal
scans):

| set | readings | model = console |
|---|---|---|
| uvmeas: first, flatscan, steep, wflat, general, recip, recipscan, reciprow, reciprown | 8,040,053 | 8,040,053 |
| wmeas: general, flatedge, flatscan, slopes | 2,320,996 | 2,320,996 |
| cmeas (colors; all three channels of a pixel) | 780,000 | 780,000 |
| uvspan: opaque list, translucent list | 818,350 | 818,350 |

Before this work refsw2 matched 31.6 % of the first uvmeas set and 94.5 % of the general wmeas set.

Not measured: how the TSP samples when the half-pixel bits of HALF_OFFSET are off (the start of a run was
only measured at pixel centers); an attribute plane that is zero everywhere on a triangle of negative
determinant (the model makes it a hair negative).

## uvspan: where a run starts, and the grid of a tile (2026-11-16)

    python uvspan/gen.py [--seed N] [--list op|tr]
    bash run.sh uvspan <ip> [emulator dir]
    python uvspan/read.py /tmp/pvr_tests/uvspan [hw|emu] [out.npz]

The two rules that were missing for general triangles, both found in the archived uvmeas data and then
tested on their own:

1. **A run starts at the first pixel of the triangle, not at the tile's edge.** The quad triangles of
   uvmeas always cover a line from the left edge of the cell, so "the first pixel of the line" and "the
   first pixel of the tile" were the same thing. On small general triangles they are not, and taking the
   first covered pixel lifts the general set from 88.4 % to 99.2 %.
2. **The grid is 22 bits under an exponent E of the triangle in the tile**, the largest of
   `exp(C)`, `exp(A) + exp(X) - 1` and `exp(B) + exp(Y) - 1` (exponents as frexp gives them: 16 has
   exponent 5), with X and Y any coordinate of the far half of the tile (tile + 16 .. tile + 32; they all
   have the same exponent). So it is the exponent a product CAN have in this tile that counts, not the
   size it has: with a slope of 1.08 (exponent 1) at x = 544 (exponent 10) the grid is one bit coarser
   than C's although A x = 588 is no larger than with a slope of 2.5 at x = 224, where it is not. C, the
   two products and the step are all rounded down to this grid. In the first tile column or row X or Y
   is 16 .. 32, exponent 5: there a slope above 1/16 of C already coarsens the grid, which is the "step
   of 2^(exp(B) - 18)" seen on steep y-only planes. This takes every uvmeas, wmeas and cmeas set to 100 %.

uvspan then asks what "the first pixel" is when a nearer polygon hides part of the line. Every cell has one
steep textured triangle and red bands over whole columns in front of it; a triangle is picked for the
number of pixels on which "first covered pixel" and "first visible pixel of the run" predict different
texels (about 75,000 per run).

| list | visible readings | first covered pixel | first visible pixel of each run |
|---|---|---|---|
| opaque (depth test and write) | 411,287 | 336,408 (none of the 74,879 that differ) | 411,287 |
| translucent, drawn in submission order | 407,063 | 407,063 | 334,486 (none of the 72,577 that differ) |

**A run is a stretch of pixels of one triangle in the pass being shaded.** In the opaque list the ISP has
removed the hidden pixels before the TSP runs, and the triangle starts anew to the right of every band.
In the translucent list (no autosort) every polygon is shaded in its own pass, in which the triangle is
whole; the band is drawn over it afterwards. refsw2 finds the runs in its tag buffer (`RenderParamTags`)
and matches both lists reading for reading. Not measured: whether a run also ends where the modifier volume
state changes, and the punch-through passes; refsw2 ends it there.

## State of refsw2 after zmeas / wmeas / uvmeas / cmeas / uvspan (2026-11-16, uncommitted working tree)

The older comparison tests, scored against the same console dumps as before
(`bash score.sh /tmp/nulldc3-fix <python> <tests>`). "2026-11-15": the build with the measured depth setup,
TSP planes for quad triangles, reciprocal table and color rounding. "now": with the run start and the
tile's grid exponent of uvspan, and the inside test and the ISP depth of covmeas / hugepos in soft floating
point (see "model" below).

| test | before the measurements | 2026-11-15 | now |
|---|---|---|---|
| texfmt, sample, depth, raster, env, alphatest, fog, bump, sort, sprite, halfofs | all regions | all | all |
| shade | 148 of 149 | 148 | 149 |
| fb565 / fb555 | 515 / 515 of 516 | 515 / 515 | 516 / 516 |
| tspw | 1563 of 1920 | 1587 | 1920 |
| zblock | 1344 of 1440 | 1344 | 1440 |
| ztile | 780 of 960 | 756 | 960 |
| precision | 6179 of 6180 | 6179 | 6180 |
| mip | 1019 of 1083 | 1019 | 1019 |
| modvol | 171 of 192 | 171 | 187 |
| huge | 195 of 250 | 193 | 195 |
| blend | 238 of 256 settings | 238 | 238 |

Every test that looks at a depth, a 1/w, a color, a texture coordinate or the edge of an ordinary polygon is
now exact (the last `precision` region was an edge passing through pixel centers). What is left is something
else each time: `mip` (the level choice and trilinear on non-square textures), `modvol` (volume cases),
`huge` (infinite, NaN and denormal inputs, and corners at 2^120: see hugepos), `blend` (18 settings).

`sample` still passes with the special case for coordinates on a texel boundary removed from the texel
address: the boundary behaviour now comes out of the division.

The direct measurements, emulator against console: ISP depth 7195 of 7195 (zero-crossing set); uvmeas,
wmeas, cmeas and uvspan every reading (see the tables above); covmeas every pixel of all 24 runs (below).
The stored cases of `ops` (random general triangles, one probe pixel each), predicted again with the final
model: depth 167 of 167, fog (1/w) 169 of 169, color 95 of 95, coverage 61 of 61.

Coverage next to an edge was the last step that only had pass / fail tests; see covmeas below.

## covmeas: which pixels are inside, when an edge passes through pixel centers (2026-11-16)

    python covmeas/gen.py [--seed N] [--mix lattice|axis|skew|diag|tile|lean|leantile|big|sliver|degen|tiny]
                          [--prim tri|strip|sprite|modvol|zprobe] [--list op|tr|pt] [--halfofs N]
    bash run.sh covmeas <ip> [emulator dir]
    python covmeas/read.py /tmp/pvr_tests/covmeas [hw|emu] [out.npz]

(`--prim zprobe` takes `--probez Z`, the depth of its green polygon, default 0.5.)

Whether a pixel is drawn is one bit, so the test makes many pixels whose bit is undecided in real arithmetic:
every cell has a white triangle with one edge along a lattice direction (p, q), small coprime integers,
through a pixel center, so the edge function is zero, or a few units of the last bit away from zero, at a
handful of pixels. Vertices are plain floats (`exact`), moved by a few units of the last bit (`ulp`), or on
the 1/16 grid and exactly on the line (`sixteenth`). The mixes:

| mix | what |
|---|---|
| `lattice` | lattice edges of any direction |
| `axis` | horizontal and vertical edges only |
| `skew` | those with an end moved across the line by up to 12 units of the last bit |
| `lean` | a nearly horizontal or vertical edge across most of the tile, leaning by 1/4 of the last kept bit of its slope up to 1 pixel in 16, placed with the model so that the edge function is close to zero at the tile's first column or further along |
| `tile`, `leantile` | triangles of 3 x 3 cells: the edge crosses two tile borders |
| `big` | wedges in the cells along the screen border with two corners 40 to 30,000 pixels outside the screen |
| `sliver` | the third corner 0.00001 to 0.05 pixel off the lattice edge, or on it |
| `degen` | exactly degenerate: a corner repeated, three equal corners, three corners on one line |
| `tiny` | three corners within 1 unit of the last place to 0.1 pixel of one point |

`--prim` draws the same triangles as part of a strip of two, as three corners of a sprite, or as a modifier
volume each (in front of a white polygon with cheap shadow); `--list` picks the list; `--halfofs 6` clears
the ISP's bit of HALF_OFFSET, and the special edges then pass through pixel corners, where the ISP samples.
Archived runs: `fit/covmeas_data`.

A console's answer is the same along the whole edge: the edge function is stepped exactly from one value per
tile, not recomputed per pixel. The model (`ops/model.py`: `edge_values`, `covered`):

1. **The constant is the depth setup's**: `c = trunc(xb ya) - trunc(xa yb)` in 24 bit arithmetic on absolute
   coordinates. Forms built from coordinate differences are far off (77 %).
2. **The winding is the sign of the setup's determinant**, the sum of the three constants (24 bit, order
   (e1 + e2) + e3), not of the exact area. They differ when a corner is huge (hugepos below) and for slivers
   (below).
3. **The value at the tile's first sample point** is `(dx y0 - dy x0) + c`, the two products and the two sums
   each cut to 24 bits, and is then kept as a **20 bit float** (cut toward zero), as the depth's start is.
   In the first sum **an operand whose exponent is 25 or more below the other's is dropped**: it is beyond
   the adder's alignment shift. (Otherwise the sum is exact before the cut, as with a sticky bit.) This
   shows in the first column and the first row of tiles, where x0 or y0 is 0.5 and its product tiny.
4. **The slopes are 20 bit floats**, both cut to 21 bits under the exponent of the larger, again as for the
   depth. A pixel's value is the start plus whole steps. Call E the exponent of the larger slope plus 6 (or
   that of the start value plus 1, if that is more) and G = 2^(E - 27) the unit of that grid.
5. **Zero is a zone**: a value of at most 64 G, half a unit of the 20th bit under E. A pixel center up to
   about 2^-14 pixel from the edge is "on" it.
6. **A row is decided at once when it starts at zero.** R is the value of the row at the tile's FIRST column.
   If R is zero, every pixel of the row in this tile gets the same answer, the one the top-left rule gives
   for an edge through the pixel centers: the sign of the column step, or, when the column step is zero as
   well (at most 64 G), the sign of the row step (top edge in, bottom edge out). The pixels' own values are
   not looked at: a nearly horizontal edge that leans by up to 64 G per pixel is in, or out, along the whole
   row of the tile, although its exact value is many zones away at the far end.
7. **Otherwise the sign of the pixel's value decides, and a pixel whose value is zero is inside when the value
   of the next pixel to its right is not negative** (zero counts as not negative). For an ordinary left edge
   the next pixel is positive, for a right edge negative: the top-left rule. But on an edge that leans by
   less than a zone per pixel, the tie pixels are inside whichever way it leans, top or bottom, up to the
   last one before the values go negative. Two triangles that share such an edge both draw those pixels.
8. **The last pixel of a tile has no next one**: a tie there goes by the sign of the column step.

How it was found: every leaning row of the first runs fitted one of two readings, "the whole row is a
horizontal edge's" or "the column step counts, and ties look one pixel ahead", and which one was decided by
|R| <= 64 G in 955 of 960 rows; the other 5 were in the first tile column, where the dropped operand moves R
by one unit of the 24 bit sum.

Model against console, every pixel of every cell (2,457,600 per run, 2,211,840 for the 3 x 3 mixes, 540,672
for `big`), and refsw2 against console over the whole frame:

| run | what | pixels within 0.001 px of an edge | model != console | refsw2 != console |
|---|---|---|---|---|
| axis2, cov1, skew3, tile4 | the runs the first model was fitted on (it missed 0, 95, 1038, 32) | | 0, 0, 0, 0 | 0, 0, 0, 0 |
| lean5 | `lean`, fitted on | 43,651 | 0 | 0 |
| lean6, lean11 | `lean`, new seeds | 43,969, 44,127 | 0, 0 | 0, 0 |
| leantile7 | `leantile` | 12,396 | 0 | 0 |
| lattice8, skew9, tile10 | new seeds of the old mixes | 22,264, 29,438, 5,929 | 0, 0, 0 | 0, 0, 0 |
| strip12, strip13 | strips of two triangles (`lattice`, `lean`) | 49,682, 55,232 | 0, 0 | 0, 0 |
| sprite14, sprite15 | sprites (`lattice`, `lean`): four edges, winding from the first three corners | 32,442, 32,852 | 0, 0 | 0, 0 |
| modvol16, modvol17 | modifier volume triangles (`lattice`, `lean`) | 47,783, 53,562 | 0, 0 | 0, 0 |
| tr18, pt19 | translucent and punch-through lists (`lean`) | 53,986, 54,132 | 0, 0 | 0, 0 |
| half20, half21, half23 | HALF_OFFSET 6: the ISP samples at pixel corners (`lattice`, `lean`, `leantile`) | 48,169, 53,851, 12,747 | 0, 0, 0 | 0, 0, 0 |
| half22 | HALF_OFFSET 1: only the ISP's bit set (`lean`) | 53,720 | 0 | 0 |
| big24 | corners up to 29,212 pixels outside the screen | 4,520 | 0 | 0 |

Every single change of a parameter is worse on these runs: the tie step threshold at 63 (30 pixels), 32
(375), 128 (246); the adder's reach at 24 (3) or 26 (34); the zero zone at 63 (1122) or 65 (32); the block
exponent, the mantissa widths and the pair grid by hundreds to thousands. (65 for the tie step is as good as
64: no step of exactly 65 G with a row starting at zero is in the data.)

### Slivers: the row range, a zero determinant, and garbage colors

Mix `sliver` puts the third corner 0.00001 to 0.05 pixel off the lattice edge (or exactly on it). The area is
then at or below what the 24 bit products resolve, and the truncated edge functions accept pixels well beyond
the ends of the triangle. Two more rules came out of it:

9. **A determinant of zero counts as positive.** Of 24,000 sliver triangles about 5,500 have a determinant of
   exactly zero in the setup's arithmetic and 1,041 one whose sign is opposite to the exact area's. The
   console follows the setup's determinant in all of them (4,128 pixels drawn by the opposite-sign ones; the
   exact winding would draw 7 of those), and with zero it takes the winding of a positive one.
10. **A polygon is rasterized on the rows floor(ymin) .. floor(ymax + 0.5) only**, ymin and ymax over its
    corners (all four of a sprite). No pixel above the row of the top corner is ever drawn (0 of 635 the edge
    functions accept), none two rows below the bottom corner's (0 of 483), and the row after the bottom
    corner's exactly when the fraction of ymax is 0.5 or more (72 of 72 against 0 of 70). The rule is the same
    in row numbers with HALF_OFFSET 6 (sliver35: without the range 1053 pixels too many). It is the ISP's:
    with `--prim zprobe` (the sliver writes its depth, a green polygon behind it follows) the rows outside
    the range are green, the depth was never written. **Modifier volume triangles have no such range**
    (sliver36: edges alone give every pixel right, the range would lose 1256). There is no such limit on
    columns.

**What "drawn" means for a sliver.** Its planes are divided by a determinant of a few units of the last bit,
so the depth and the color of a sliver are garbage: a flat white one comes out 255 on 6473 pixels of
sliver30, between 192 and 254 on 2135, between 64 and 191 on 26, never 0 there. (The TSP color model of
`ops/model.py` gives the console's value on 8588 of those 8634.) `read.py` therefore counts every pixel that
is not black as drawn; with white above 128, as it was first, 75 pixels of the sliver runs looked missing,
e.g. a whole column of (111, 111, 111). Read that way the model's coverage differs from the console on 7 of
29,491,200 pixels of the twelve sliver runs with polygons, and those 7 are covered too: the color model puts
them at 0, and the depth probe finds their depth written (with the probe polygon at 0.5 for the three whose
depth the model puts at 0.75 and 1.5, at 0.01 for the three it puts at 0.11).

| run | what | model != console |
|---|---|---|
| sliver25, 27, 30 - 34 | slivers, opaque triangles | 0 |
| sliver28, sliver29 | the same | 3, 3: covered, black (depth written) |
| sliver26 | slivers as the first triangle of a strip | 1, of that kind |
| sliver35 | slivers, HALF_OFFSET 6 | 0 |
| sliver36 | slivers as modifier volumes (no row range) | 0 |
| sliver37 | slivers in the translucent list | 0 |
| zprobe38, 27, 29, 32, 28b | depth probe on sliver25, 27, 29, 32 (probe at 0.5) and 28 (at 0.01) | depth written on exactly the model's pixels |
| zprobe30, 30b | depth probe on sliver30 at 0.5 and 0.01 | 4 pixels the white run shows drawn have a depth below 0.01 |

Left over, and not coverage: the model's depth is negative at those 4 pixels of sliver30, which a console draws
although it otherwise leaves out pixels with a negative depth; and the color model is 0 at 870 sliver pixels a
console shows (its division by a zero or negative 1/w is not measured).

### Degenerate triangles without culling: the hardware draws rows of the tile

    python covmeas/gen.py --mix degen [--prim strip]      # corners repeated, or exactly on one line
    python covmeas/gen.py --mix tiny                      # three corners within 1 ulp .. 0.1 pixel of one point
    ... [--cull none|small|ccw|cw] [--z Z | --zs z1,z2,z3] [--vcol flat|gouraud] [--prim zprobe --probez Z]

With culling off (`PVR_CULLING_NONE`) a console rasterizes triangles that have no area, and what comes out is
not nothing:

- **Three equal corners draw the whole width of their tile**, 32 pixels, on the rows of the row range
  floor(y) .. floor(y + 0.5): one row when the fraction of y is below 0.5, two otherwise (degen40: 234 of 234
  such triangles, 11,456 pixels). An edge between two equal corners has a function that is zero everywhere, and
  zero everywhere counts as INSIDE; with all three edges like that only the row range and the tile limit the
  triangle.
- **A corner repeated once** (A A B, A B B, A B A) and **three corners exactly on a line** draw nothing: the two
  real edges run opposite ways and disagree on every tie. The same inside a strip (degen41: A A B C, A B C C,
  A B B C, C A B A, with the middle point too): only the strip's real triangle shows.
- **Three corners that are almost one point** (mix `tiny`) draw anything from nothing to whole rows of the tile:
  their edge constants are pure rounding of 24 bit products. 395 of 1200 such triangles draw something in
  tiny42, 13,530 pixels.
- **The depth of a triangle whose determinant is zero is the mean of its three corners' depths**, at every pixel:
  three equal corners, or a sliver whose constants cancel exactly although each is in the thousands. With
  corner depths 0.2, 0.7, 0.3 all 11,456 + 1,344 + 53 such pixels store a depth in [0.35, 0.45); finer brackets
  on the points: [0.39, 0.41) there, [0.55, 0.58) for 0.9, 0.3, 0.5, [0.3, 0.4) for 0.1, 0.1, 0.9 and for
  0.9, 0.1, 0.1. (The arithmetic of that mean is not measured beyond these brackets.)
- **Its color is the third corner's**, Gouraud shaded or flat (corners red, green, blue: the 11,456 pixels are
  (0, 0, 255) either way).
- With any other culling mode (small, clockwise, counter-clockwise) none of these is drawn.

The model needed one change for this, the zero edge counting as inside; the depth unit one, the mean; refsw2's
TSP planes one, the third corner. The rest is what the soft floating point already does:

| run | model != console | refsw2 != console (exact colors) |
|---|---|---|
| degen40: degenerate triangles (2400, 234 of them points) | 0 | 0 |
| degen41: strips with a degenerate triangle | 0 | 0 |
| tiny42, tiny43: almost points | 0, 0 | 0 |
| points and almost points, corners red / green / blue, Gouraud | | 0, 0 |
| slivers (sliver25), corners red / green / blue, Gouraud | | 0 |
| zd40: depth probes at 10 depths on the points, corner depths 0.2, 0.7, 0.3 | 0 at every depth | 0 (at 0.35, 0.45) |
| zt42: the same on the almost-points (13,530 pixels) | 0 at every depth | 0 (at 0.35) |
| zs25: the same on the slivers (7,848 pixels) | 51, 51, 51, 35, 21, 13, 5, 3, 1, 0 | |
| points, almost-points, slivers with culling small / ccw / cw | | 0 (nothing drawn but 177 sliver pixels) |

Open: the depth of slivers whose determinant is NOT zero and whose corners have different depths (zs25). Their
planes have slopes of a hundred per pixel and a start value thousands of times the result; the model is inside
the console's 0.1 wide bracket on 7,176 of 7,795 such pixels and misses the rest by 0.02 or less mostly. With
equal corner depths (every earlier sliver probe) nothing is off.

### hugepos: corners at 2^30 to 2^125

    python hugepos/gen.py [--tri all|a|b|c] [16 exponents]
    bash run.sh hugepos <ip> [emulator dir]
    python hugepos/read.py /tmp/pvr_tests/hugepos [hw|emu]

Three triangles with M = 2^m: a, three huge corners (-4M, -3M) (4M, 3M) (-3M, 4M); b, one huge corner
(-64, -64) (704, 0) (-3M, 4M); c, two corners huge in x only (-4M, 100) (4M, 380) (320, -1000). One per page
in the opaque list (`--tri a|b|c`):

| | console draws exactly the pixels of exact arithmetic | then |
|---|---|---|
| a | up to 2^116 | 2^118: 15,019 of 153,600 (the tiles near the origin), 2^120: 705 (24 of them outside), 2^122: 15 |
| b | up to 2^114 | 2^116: 110,116 of 248,789 (only where x <= y), 2^118: 30,288, 2^120: 2,062 |
| c | up to 2^112 | from 2^114: nothing |

- **The winding comes from the determinant.** For b the exact area is (3M)(-4M) - (-4M)(3M) plus terms that
  are 50 bits smaller: zero in double precision from 2^62 on. refsw2 took the winding from that and drew
  nothing from 2^62 to 2^114; with the setup's determinant it draws what the console draws.
- **The tile's start value saturates; the constant does not.** When the sum dx y0 - dy x0, or the start value
  itself, reaches 2^128, the start becomes 2^127 with the sign of what overflowed. The constant c is fine
  far beyond that (25 M^2, about 2^245). So for a at 2^118 the console draws exactly the tiles where the sum
  stays below 2^128 for the edge whose constant has the other sign; at 2^120 the start of -2^127 is small
  enough for the steps to cross zero inside the tile, and the edge appears 18 rows down the first tile,
  with a few pixels in tiles where nothing should be. The model gives the console's pixels exactly for a at
  2^118, 2^120, 2^122 and 2^124 (15,019, 705, 15 and 2 pixels; at 2^125, where a coordinate difference
  itself overflows, 1 of the console's 4).
- refsw2 (soft floating point build) draws the console's pixels for a at every exponent up to 2^124 (15,019,
  705, 15, 2), and for b and c up to where their depth limit starts.
- Not modelled: the limits of b and c. They follow the determinant reaching about 2^127 (its reciprocal
  leaves the float range) and are the depth's doing, not the edges': c, whose depth slopes are zero and
  whose determinant is negative, is not drawn at all; b, whose depth slopes are -64 and 768 before the
  division and whose determinant is positive, is drawn exactly where x <= y (110,116 pixels at 2^116), as a
  depth of the form huge * (y - x) with negative values not drawn would be. Above 2^116 the edges' limit
  cuts b further.
- `--tri all` puts the three into the translucent list, one color channel each, added. From 2^62 the green
  and blue triangles also show inside the red one's area, where they do not cover: the translucent sort goes
  wrong when depths are garbage. Not looked into.

## model: one implementation in soft floating point, for the emulator and the probes (2026-11-16)

The hardware's arithmetic is not IEEE: mantissas are cut toward zero, adders reach only so far when they align,
overflow saturates. Emulating that with doubles works until one limit is forgotten, and every bug of the
covmeas / hugepos round was of that kind: a winding taken from an exact double area, a sum that cannot overflow,
an adder that keeps an operand the console's shifter drops. So the solved units are now written once, in C++,
on a soft floating point with an explicit sign, exponent and integer mantissa, where every operation says how
many bits it keeps:

| file (nulldc3 `libswirl/rend/refsw2/`) | what |
|---|---|
| `pvr_softfp.h` | the number type: `Mul(a, b, bits)`, `Add(a, b, bits, reach)` (exact, then cut; an operand `reach` exponents below the other is dropped), `cut`, `cutBelow`, conversion from a float's bits |
| `pvr_isp_edge.h` | the ISP's inside test: winding from the determinant, the edge per tile (`EdgeTile`), rows and tie pixels, the row range; `EdgeConfig` holds every width and threshold |
| `pvr_isp_depth.h` | the ISP's depth per tile (`DepthTile`), `DepthConfig` |
| `pvr_tsp_plane.h` | the TSP's planes (`Plane`: 16 bit coefficients, one evaluation per run of pixels on the tile's 22 bit grid, rounding down, signed zeros), its table reciprocal and `Divide` |

The units share the number type and nothing else: the TSP is another block with other rules (it rounds toward
minus infinity where the ISP cuts toward zero, keeps 16 bits where the ISP keeps 20, gives a zero determinant
the third corner's value where the ISP's depth takes the mean), so each header carries its own configuration,
and nothing measured on one is assumed for the other. (The one exception was the adders' reach, measured on the
ISP only: the TSP's setup sums are left exact.) The hardware goes ISP -> spans (created and sorted) -> TSP; the
"run" of `uvspan` is such a span, and `Plane::Value(x0, y, n)` is written for one: its start and a pixel count.

refsw2's rasterizer uses `Winding`, `EdgeTile` and `DepthTile` directly (it is also faster that way: a
covmeas run takes 18 s in the emulator, 25 s with the doubles), and its `PlaneStepper3` evaluates through
`pvrtsp::Plane` and `pvrtsp::Divide` (the doubles remain only for infinite and NaN inputs). The same headers, compiled alone, are the model
the probes run against the archived console data:

    python model/export_cov.py        # fit/covmeas_data/*.npz  ->  *.cov
    python model/export_z.py          # fit/zmeas_data/*.json   ->  *.zme
    bash model/build.sh
    model/covprobe [name=value ...] [--scan name=v1,v2,...] [--misses N] fit/covmeas_data/*.cov
    model/depthprobe [name=value ...] [--scan name=v1,v2,...] [--misses N] fit/zmeas_data/*.zme

`covprobe` rasterizes every polygon of a run into its page and compares the page with the console's; all 42
archived runs (103 million pixels) take 2 seconds. With the default configuration it gives what the Python model
gives: no difference on the 24 runs before the slivers, on ten of the thirteen sliver runs and on five of the
seven depth probes; the other 7 + 8 pixels are the black and garbage-depth pixels of the sliver section. `--scan` varies one parameter:

| parameter (model's value) | pixels that differ, summed over 30 runs, for the values tried |
|---|---|
| reach (25) | 23: 1023, 24: 92, 25: 0, 26: 160, 27: 314 |
| zeroShift (21) | 20: 67,669, 21: 0, 22: 51,964 |
| tieShift (21) | 20: 2730, 21: 0, 22: 3137 |
| pairBits (21) | 20: 3729, 21: 0, 22: 875 |
| startBits (20) | 19: 16,830, 20: 0, 21: 7035 |
| blockAdd (6) | 5: 37,105, 6: 0, 7: 70,706 |
| sumBits (24) | 23: 28,770, 24: 0, 25: 13,698 |
| productBits (24) | 23: 142,797, 24: 0, 25: 110,672 |
| lastColumnByStep (1) | 0: 246, 1: 0 |
| rows (1) | 0: 5777, 1: 0 |

`depthprobe` evaluates the stored depth at every measured pixel of zmeas: inside the console's bracket on 50,279
of 50,280, as the Python model. One step off each way: recipBits 23 / 25: 206 / 78 wrong; startBits 19 / 21:
1137 / 594; pairBits 20 / 22: 187 / 41; tileBits 25 / 27: 472 / 232; sumBits 23 / 25: 3480 / 2460; productBits
23 / 25: 4330 / 3734. An adder reach of 24 or more makes no difference to the depth data (20: 673 wrong), so the
depth unit is compatible with the 25 measured on the edge unit.

refsw2 built on these headers against the console, whole frames: no pixel differs on 36 of the 37 covmeas
runs (88 million pixels; triangles, strips, sprites, modifier volumes, three lists, both sample offsets, corners
30,000 pixels off screen, slivers) and on the five depth probes, in color and in where depth is written; the
exception is 4 pixels of one sliver in sliver30 whose depth the model has negative (the emulator leaves them
out, the console draws them). hugepos: the same pixels as the console for triangle a up to 2^124, for b up to
2^114 and for c up to 2^112. The older tests: `modvol` went from 171 to 187 of 192 regions and `huge` from 193
to 195 of 250 with the new inside test; nothing else changed.

refsw2 with the TSP planes in soft floating point against the archived console readings: uvmeas ugen5
240,053 of 240,053, uwflat4, urecip6 and urscan7 780,000 of 780,000 each, cmeas c1 780,000 of 780,000, wmeas w1
695,796 of 695,796.

Done since: the probe for the TSP archives (`model/tspprobe`) and the test generators (see "The generators are
C++ programs on the refsw2 headers"); `ops/model.py` no longer exists. Still to do on this path: refsw2's
shading as a real span pass.

## Direct harness and refsw2-offline: one source for the offline renderer, a console and the emulator (2026-11-16)

The tests above run on KallistiOS's PVR driver and need the whole emulator for the refsw2 side: one emulator
start per page, about 18 s for a set. Two things replace that for new tests.

**`nulldc3/refsw2-offline`** (`main.cpp`, `build.sh`; uncommitted) is the refsw2 renderer and the lxdream
tile accelerator without the rest of the emulator. It starts with every register and all of video memory at
zero and does what a stream of commands says; one process renders any number of frames.

    refsw2-offline [--lax] [-v] [-c "cmd; cmd"] [--dump <folder> [png]] [script | -]...

    reset                        registers and video memory back to zero
    reg <name|offset> <value>    a register write as the CPU does it (TA_LIST_INIT, SOFTRESET, STARTRENDER act)
    w32 <address> <word>...      video memory, 32 bit area addresses
    w64 <address> <word>...      video memory, 64 bit area addresses (textures)
    ta <word>...                 to the tile accelerator, a multiple of 8 words
    render                       reg STARTRENDER ffffffff
    wait <events>                checks that these interrupts happened (a console waits for them)
    dump32 <address> <words> <file>     words of the 32 bit area, as they are
    png <file>                   the displayed picture (FB_R_*)
    log <file> | log off         refsw2's render log
    regs <file>, vram <file>, save <folder>     the emulator's dump format (pvr_regs.bin, vram.bin)

All numbers are hexadecimal. `--dump <folder>` renders a dump made by the emulator, like the old
refsw-offline. Nothing is initialized behind the caller's back: a render or a list initialization that reads
a register nobody wrote is an error (`--lax`: a warning) and the render is not done, and so is a region
array without a last entry. The fog table and the palette are not tracked.

**The direct harness** (`common/pvr_params.h`, `pvrio.h`, `pvrsetup.h`, `pvrtest2.h`; `run2.sh`,
`compare2.py`) does not use KallistiOS's PVR driver at all (its functions are poisoned in these builds).
Everything a test does goes through six calls,

    pvrio_reg(addr, value)   pvrio_w32(addr, words, n)   pvrio_w64(addr, words, n)
    pvrio_ta(params, words)  pvrio_wait(events)          pvrio_dump32(addr, words, name, suffix)

which have two implementations: built for a PC they print the commands above; built for the Dreamcast they
write the registers, video memory and the store queues themselves (interrupts through KallistiOS's handler,
the display mode through its video driver). The same Dreamcast program runs in the emulator.

    bash run2.sh covmeas off                     # PC build | refsw2-offline        -> page<n>.off.raw
    bash run2.sh covmeas hw=192.168.178.107      # console                          -> page<n>.hw.raw
    bash run2.sh covmeas emu                     # the console's program in nulldc3 -> page<n>.emu.png
    bash run2.sh covmeas fbpack off hw=<ip>      # several tests
    bash run2.sh covmeas@lattice8 covmeas@strip12 off      # archived sets of one test, side by side
    python3 compare2.py covmeas hw off           # word for word, per labeled region

With several tests the offline runs go in parallel, one per CPU core (`-j n` for another number); the
console does one after the other; the emulator's pages run as many at a time as there are cores.
`<test>@<set>` gives a run its own folder and its own tables (`regen.py <test> <set> <folder>` writes them
there from `fit/<test>_data`, and the test includes `CASES_H`), which is what lets sets of the same test run
at the same time. `bash archive_check.sh <test> [sets]` does that for every archived set of a test and
compares each with the console's archived result (`compare_archive.py`).

How the harness keeps the three sides on the same input:

- **Parameter words are set field by field**, with nulldc3's names: `PCW`, `ISP_TSP`, `TSP`, `TCW`,
  `TA_PolyParam0`, `TA_Vertex0`, `TA_SpriteParam`, `TA_ModVolParam`, `RegionArrayEntry`,
  `ISP_BACKGND_T_type`, `FB_W_CTRL_type`, registers as `HALF_OFFSET_addr`. There is no context that fills
  in defaults.
- **No float arithmetic at run time.** A float is carried as its bit pattern (`f32b`); a test's generator
  writes them precomputed into the test's tables (`covmeas/gen.py` does), and `f32b_int()` / `f32b_scaled()`
  make a number from integers with integer operations only. So the SH4 and the PC cannot round differently,
  whatever the compiler does.
- **Every call is fed to a checksum**, printed before each dump (`PVRIO crc ...`). `run2.sh` compares the
  lines of the offline run and the console run: equal lines mean the two rendered exactly the same register
  writes, memory contents and tile accelerator input, in the same order.
- **Every register is written explicitly** (`pvrsetup.h`), with the value a console has when a test runs
  under KallistiOS's driver. Those were read from a console with `regdump/` (which also prints what the
  boot ROM and the video driver left). Differences found on the way:

  | register | console under KallistiOS | emulator under KallistiOS | written now |
  |---|---|---|---|
  | SCALER_CTL | 0x400 (VGA cable) | 0x401 (it reports a composite cable) | 0x400 |
  | PAL_RAM_CTRL | 3 (left by the boot ROM) | 0 | 0 |
  | FB_W_SOF2 | 0x600000 (left by the boot ROM) | 0 | the framebuffer |
  | ISP_BACKGND_D | reads back 0x38d1b710 after 0x38d1b717 is written: the low 4 bits do not exist | 0x38d1b717 | 0x38d1b717 |
  | fog table, palette | whatever was there | 0 | 0 |

- **Video memory is laid out by hand**, the structures as KallistiOS writes them: the region array has 18
  zero words and one entry that draws nothing (`no_writeout`, all lists empty) in front of the 300 tiles,
  which are written column by column; every list ends with a polygon parameter without vertices before
  the end of list. Without the extra entry the hardware sometimes hangs. The background plane's parameters
  are at a fixed address behind the tile accelerator's parameter memory, so nothing has to be read back
  from the chip to place them. One framebuffer: what is rendered is displayed and is what the dump reads,
  directly after the render ends (no waiting for the display).
- A console dump is run-length coded when that is shorter (`.rle`, expanded by `run2.sh`).

### Results

Ported so far: `covmeas`, `uvmeas`, `uvspan`, `wmeas`, `cmeas`, and the new `fbpack` (in each, `main_kos.c` is
the earlier version, and the parameter words the new `main.c` sets field by field are the ones KallistiOS
compiled for the old one). Each of them, run on the console through the new path, gives the console's archived
readings again, and console and offline agree on every framebuffer word of those runs.

    bash archive_check.sh all        # every archived console set of the ported tests, 26 s
    bash measure.sh covmeas <new set name> --seed 44 --mix lattice     # a new measurement, 7 s: generate, console,
                                     # offline, archive the console's readings, compare

| test | archived console sets | equal to the console through refsw2-offline |
|---|---|---|
| covmeas | 79 | 69; the other 10 are the open sliver cases (sliver30: 4 pixels; the zs25 ladder) |
| uvmeas | 24 (18.2 million readings) | all |
| uvspan | 2 | all |
| wmeas | 4 (2.3 million readings) | all |
| cmeas | 1 (2.3 million colour values) | all |

uvmeas's usteep3, uv1 and uflat2 could not be run again in the emulator before (their archives lack the
depths the old generator needed); they are checked now.

Times, for covmeas:

| | before | now |
|---|---|---|
| console run of a set (8 pages) | 14 s | 5 s |
| refsw2 side of a set | 18 s (emulator) | under 1 s (offline) |
| all 79 archived sets against the console's archived results | about 25 minutes | 12 s on 32 cores (`archive_check.sh covmeas`) |

The emulator running the console's program (`run2.sh covmeas emu`) gives the console's picture too. After the
fbpack change the old-harness tests that depend on the framebuffer writer are unchanged in the emulator:
fb565 516/516, fb555 516/516, texfmt 540/540, shade 149/149, alphatest 960/960, blend 238/256.

### fbpack: what is written to the framebuffer in each pixel format

17 pages with the same picture (20 colours and alphas; opaque, translucent, blended, punch through, with and
without alpha) and another FB_W_CTRL each. Found on the console, and now in refsw2 (`refsw_lists.cpp`):

- **0888 (packmode 5): the top byte is `fb_kval`**, not alpha. refsw2 wrote the alpha.
- **0555 (0): bit 15 is bit 7 of `fb_kval`.** refsw2 wrote 0.
- **1555 (3): bit 15 is "alpha >= `fb_alpha_threshold`"**, not dithered. **4444 (2): alpha and colours cut
  to 4 bits.** refsw2 had neither format (it asserted).
- **8888 (6): the pixel's alpha**; no dithering in the 24 and 32 bit formats.
- **Dithering is one 4x4 table of 16 levels**, added before the low bits are dropped, saturating: the table
  itself for 4 bits, half of it (rounded down) for 5 bits, a quarter for 6 bits. In 4444 the alpha is
  dithered like the colours.

       5 13  7 15
       9  1 11  3          value added at (x & 3, y & 3), rows are y
       6 14  4 12
      10  2  8  0

  Every entry is pinned by the data (lower bound = upper bound). The 5 bit table refsw2 already had from
  fb555 is this one halved.
- An untextured punch through polygon is drawn at every pixel, also where its alpha is below PT_ALPHA_REF;
  the alpha written is 0x00 below the reference and 0xff from it on. refsw2 already did this.
- Also in refsw2: the 24 bit format written to a texture used a wrong address (`a & b + 1`).

Console = refsw2-offline on 5,222,400 of 5,222,400 pixels (`python3 fbpack/read.py hw off`).

### Every test is on the direct harness now; regress.sh replaces the emulator scoring (2026-11-16)

All tests were ported from `pvrtest.h` (KallistiOS's driver) to `pvrtest2.h`; `common/PORTING.md` is the
guide that was followed. In each folder `main_kos.c` is the original, and `fit/kos_hw/<test>` keeps the
console's dumps of the original: a port had to give the same picture on the console
(`python3 compare2.py <test> hw kos`), and every one does, in every region, with three exceptions that are
not port differences:

- blend's settings 236..255, which differ from run to run with the old program as well (87 of 4096 regions
  that time): the last 20 cells, damaged by the empty tiles after them (see blendpos below), not the settings;
- **mip: the old test was faulty** in 208 regions. KallistiOS's `pvr_txr_load` copies whole 32 byte blocks,
  so the last 12 texels of the mip texture and the 16 VQ index bytes were never uploaded (dark edge pixels,
  black VQ pictures), and its `PVR_TXRFMT_STRIDE` sets TCW bit 21 where the stride select is bit 25, so
  stride was never on. The port uploads every byte and sets `StrideSel`; with the two faults put back in it
  gives the old picture in 1082 of 1083 regions. The VQ and stride regions, tested for the first time, are
  equal on the console and in refsw2;
- **scaler needs a new test**: it sends about 21,000 quads per page (105,000 parameters), far more than the
  megabyte of parameter memory, on the old harness as on the new one (the console reports the overflow
  now: error bit 2, and some renders never finish). It is ported but not scored.

cliptest (user tile clip, which misbehaves on the console) and regdump stay on KallistiOS's driver.

What a ported test needed besides the harness:

- **`common/softfp.h`**: IEEE single precision add, subtract, multiply, divide, scale and conversions in
  integer operations (87,500,000 results checked against a PC's own floats, `softfp_test.c`). The tests that
  calculate coordinates at run time use it, so no test has a float variable left. Where an original
  computed in double and rounded once (zblock, ztile, precision, tspw), the port does the exact integer
  product and rounds once (`sf_round`).
- **`common/kos_words.sh prog.c`**: builds a PC program against KallistiOS's own header-compile code, to
  see the words a KallistiOS context made.
- fog: the fog tables and density values KallistiOS computed with libm on the console are constants in
  `fog/main.c` (from KallistiOS's `pvr_fog.c` built on a PC; the console's old picture confirms them).
- fb565 / fb555 write FB_W_CTRL themselves (nothing is read back); blend is 16 pages (page = set * 4 + view)
  with one label per blend setting and uses `run2.sh` like the others.

**`bash regress.sh`** is the regression now: every test is built for the PC, rendered by refsw2-offline
(one per CPU core) and compared region by region with the console's dumps kept in `fit/hw/<test>`
(`archive_hw.sh <test>` after a console run), then the archived sets of the measuring tests are checked
(`archive_check.sh all`). No console and no emulator; about half a minute. A test whose input is no longer
what the console was given is flagged (the checksums differ) until it has run on the console again.

Scores, refsw2-offline against the console (the emulator scored the same where it was measured):

| test | regions equal | | test | regions equal |
|---|---|---|---|---|
| texfmt | 540 / 540 | | fb565, fb555 | 516 / 516 each |
| alphatest | 960 / 960 | | precision | 6180 / 6180 |
| bump | 640 / 640 | | zblock | 1440 / 1440 |
| env | 208 / 208 | | ztile | 960 / 960 |
| fog | 640 / 640 | | tspw | 1920 / 1920 |
| shade | 149 / 149 | | ops | 526 / 526 |
| depth | 336 / 336 | | fbpack | 136 / 136 |
| raster | 1040 / 1040 | | modvol | 187 / 192 |
| sort | 56 / 56 | | huge | 195 / 250 |
| sprite | 64 / 64 | | hugepos | 9 / 16 pages |
| blend | 4096 / 4096 (after blendsel / blendpos, below; 3991 before) | | sample | 250 / 250 |
| halfofs | 1080 / 1080 | | mip | 1083 / 1083 (after mipmeas, below; 1019 before) |

What the differences are:

- **huge** (55 regions, all on the depth page) and **modvol** (5 regions) look like one thing: where the
  console's TSP divides by an extreme or special 1/w, colours saturate to white or come out a few levels
  low (a white cell reads fa..fb at one depth), and refsw2 gives the plain colour or black. In modvol it is
  the two-volume polygon in intensity-shadow mode: the console shows a white triangle in the first tile of
  the row, which fits the core reading that parameter block with the wrong vertex size and getting corners
  with garbage depths.
- **blend**: solved, see "blendsel, blendsel2, blendpos" below (the last 20 cells were damaged by the empty
  tiles after them; settings 193 and 196 are a bug of the blend unit that refsw2 now has too).
- **hugepos**: triangle c from 2^114 up (known).
- **mip**: the 64 regions were the trilinear ones; solved with mipmeas (below).

## mipmeas: the mipmap level and the trilinear blend, as numbers (2026-11-16)

    bash measure.sh mipmeas <new set> --mix first|w|det|det2|dcalc --seed n      # 9 s
    bash archive_check.sh mipmeas

A 64x64 mipmapped texture whose levels are flat colours; every cell is a 32x32 quad (or a triangle) of it,
drawn with ONE trilinear pass that replaces what is there. Pass B (FilterMode 3) shows the next level's
colour times the blend coefficient, so a pixel's colour is the level the TSP chose and its 7 bit fraction.
Five sets, 12,000 cells, 20.7 million colour values: refsw2 now gives every one of them (it had 39 to 59 %
of them before, and none where 1/w was not 1: it multiplied by 1/w where the hardware divides by its
square).

**The rule** (nulldc3 `pvr_tsp_plane.h`, `MipSelect`). It is the D of the hardware manual,
`D = max(dx, dy)`, `dx = sqrt(dudx^2 + dvdx^2)`, `dudx = (a r - p c) / (p X + q Y + r)^2` and so on with
`u/w = a X + b Y + c`, `v/w = d X + e Y + f`, `1/w = p X + q Y + r`, done like this:

- **Per polygon, K.** The four numerators `|A_a C_w - C_a A_w|` (along x) and `|B_a C_w - C_a B_w|` (along y)
  for a = U and V, from the planes' coefficients before they are cut to 16 bits (with the 16 bit ones, 8 of
  300 cells of one page are wrong). With **DCalcCtrl = 1** the planes' values at the polygon's first vertex
  take the place of the constants C (page 6 of mix dcalc: each corner sent first in turn, 300 of 300).
  The two numerators of a direction make one length: **the larger plus 27146/65536 of the smaller**, that
  is sqrt(2) - 1 to 16 bits, in place of the square root (27142 and 27150 are already wrong on thousands of
  pixels). K is the longer of the two lengths, times the texture's size and the bias (MipMapD / 4), **cut
  to 8 bits**.
- **Per pixel, w^2.** w is the TSP's 16 bit table reciprocal of the pixel's 1/w, **cut to 8 bits**; w * w is
  cut to 8 bits, and so is K times it. So D has 8 bits: `m * 2^L`, m = 128 .. 255 of 128.
- **Level L; the fraction is m - 128, of 128; the blend coefficient is twice the fraction, and 127 counts as
  128** (pass A: 256 - k of this level, pass B: k of the next). Below level 0 nothing is blended.

What this explains:

- The TSP's reciprocal is a hair low: 1 / 1.0 is 0.FFFF, 8 bits of that 0.FF, its square 0.FE. So a
  polygon whose 1/w is exactly 1.0 loses two units of m: it changes level at 2 + 1/32 texels per pixel, and
  at exactly 2:1 it still shows the larger level with 126/128 of blending. That is the case of every quad
  whose setup determinant is a power of two (32x32, 16x16, 32x16 ...). With any other size the 1/w plane
  comes out as 0.FFFF.., its reciprocal is 1.0, and nothing is lost: 20 kinds of triangles in mix det2 show
  it (determinant +2^n: two units; -1024: none, because there the TSP's plane value is a unit below 1.0;
  anything else: none). The old mip test only had power of two quads and 24 / 12 / 6 / 3 pixel ones, which is
  why a formula with a fixed offset looked right there.
- A flat 1/w that is not a power of two scatters the level by up to 1.5 / 128: 8 bit truncations of w
  and w * w (1485 of 1486 cells with the first 8 bit model tried, all of them with the table reciprocal).
- With perspective the level follows w^2 per pixel and jumps between the two triangles of a quad whose
  texture mapping is not one plane.

`mip` (the old test) is 1083 of 1083 regions now, was 1019. Its faulty uploads were found in the port (see
above); the level table it printed for point sampling is the same rule.

The earlier notes under "mip" about level selection, magnified quads and trilinear were of the first
refsw2 and are superseded by this section.

## scalermeas, scalertile, sof2probe: the scaler, the pixel clip and the second framebuffer (2026-11-16)

Three tests on the direct harness; every page renders the whole screen magenta without scaling first, so
what a scaled render does not write is visible. Console and refsw2-offline agree on every pixel of all of
them (`regress.sh`: scalermeas 43 of 43, scalertile 34 of 34, sof2probe 12 of 12, and the 31 dumps of second
buffers and of lines 480..959).

    scalermeas   43 values of SCALER_CTL (and of SPG_CONTROL, FB_X_CLIP, FB_Y_CLIP). Left half: one quad
                 per line; right half: one quad per column, each in its own colour.
    scalertile   34 pages: the tiles of the region array in six orders (pvr_region_write in
                 common/pvrsetup.h), one quad per pixel in six of the tiles, two frames in a row, and a
                 picture that is let through for all 1024 lines.
    sof2probe    the display interlaced, renders 1, 2 and 3 vertical blanks apart, FB_W_SOF2 pointing at a
                 second buffer.

The manual's figure (3-line filter, then scaling; "scale up repeats lines") is half right.

**Things to know before using the register**

- A console whose SCALER_CTL is changed after it has rendered a frame never finishes the next render: it
  writes the first tile and stops. Every value works when it is set before the first render, or when the
  core is reset (SOFTRESET = 2, then 0) before the change. This is why the old `scaler` test, which changed
  the value through KallistiOS between frames, was inconclusive; scalermeas replaces it.
- FB_W_SOF2 has nothing to do with interlace: it is the second strip buffer (below). With the interlace
  bit and either value of the field bit, with the display progressive or interlaced, and with renders
  falling into both fields of the display, everything goes to FB_W_SOF1 (18 + 12 dumps of the second buffer,
  not one pixel changed). `sof2field` checks the part those two tests took on trust: SPG_CONTROL reads back
  0x150 and SPG_LOAD 0x020c0359 (what KallistiOS programs for 640x480 NTSC interlaced; its vid_set_mode
  cannot be used for this with a VGA cable, it clears the interlace flag), SPG_STATUS shows the field
  changing, and each of 0x20800, 0x60800, 0x20400 and 0x60400 is rendered once starting in field 0 and once
  in field 1: the buffer at FB_W_SOF2 stays untouched all eight times. `sof2sweep` adds 34 more settings
  with the same result: each reserved bit of SCALER_CTL (19..31; they read back as 0) and of FB_W_CTRL
  (4..7, 24..31; likewise), and SPG_CONTROL 0x190 (PAL), 0x130, 0x170, 0x1d0, 0x110, 0x350, 0x050. The two
  bits of SCALER_CTL only change which lines the scaler takes; putting the fields into two buffers, or into
  alternate lines of one, is up to the software (FB_W_SOF1 and FB_W_LINESTRIDE between renders).
- **Strip buffer: FB_R_CTRL bit 22, strip size in bits 21..16, is what uses FB_W_SOF2** (`sof2sweep`;
  KallistiOS's header calls the register "output for strip-buffering"). The picture is cut into strips of
  16 x size lines; they go alternately to FB_W_SOF1 and FB_W_SOF2, each written from the first line of its
  buffer, so a buffer only ever holds one strip. With the tiles row by row and one colour per tile row:

      size 2 (32 lines)    FB_W_SOF1 ends with tile row 14, FB_W_SOF2 with tile row 13
      size 4 (64 lines)    FB_W_SOF1: rows 12, 13                FB_W_SOF2: row 14, then row 11 left from before
      size 8 (128 lines)   FB_W_SOF1: rows 8, 9, 10, 11          FB_W_SOF2: rows 12, 13, 14, then row 7

  the same with SPG_CONTROL 0x100 and 0x150, and the same in two runs. Size 0: nothing is written to either
  buffer. Oddities: one pixel of the second strip's first tile lands in the first buffer, in the line after
  its strip; with strips of 16 lines (half a tile) or with the tiles column by column, one of the buffers
  only gets the first tile's 32 columns; with the scaler on as well the result is scrambled. A render with
  the strip buffer on left the core stuck for the renders after it in one arrangement of this test, so
  those pages come last and start from a full reset. refsw2 does not implement the strip buffer, and how
  the display reads the strips was not looked at. (The console dumps of sof2field and sof2sweep are not
  archived for regress.sh: they hold registers read on the console; the strip pages are kept in
  fit/sof2sweep_hw.)
- FB_X_CLIP and FB_Y_CLIP clip what comes out of the scaler, in framebuffer coordinates, both ends included
  (refsw2 ignored them before).

**The arithmetic**, for the order of tiles KallistiOS uses (column by column, top to bottom):

- factor = the low 16 bits; 0x400 is 1:1. Output line j is taken at pos = j * factor, in 1/1024 lines:
  line k = pos >> 10 and the one below, weighted with the top 4 bits of the fraction f = (pos >> 6) & 15:
  (a * (16 - f) + b * f) >> 4, per 8 bit channel.
- Only when the picture shrinks (factor above 0x400) are the lines filtered first: (above + 2 * line + below)
  >> 2, with the first and the last line repeated. So 0x401, what KallistiOS sets for a TV, blurs every
  line 1-2-1 and drops one line; 0x800 gives every second line of the blurred picture.
- When it grows (factor below 0x400) there is no filter, and lines are not repeated as the manual's
  figure says but blended with the same 4 bit weight: 0x200 gives line, average, line, average.
- Interlace bit and field bit both set, and shrinking: pos starts at 1024, one line further down
  (0x60800 gives the odd lines, 0x20800 and 0x40800 the even ones). Either bit alone does nothing, and
  neither does both when the picture does not shrink.
- A line is written as long as the lines it needs exist: 0x401 writes 479 lines, 0x801 240.
- Bit 16: after all that, pairs of pixels are averaged, (a + b) >> 1, into half the width.
- The output line counter has 10 bits: with 0x100 (four lines out of one) lines 1024.. land on 0.. again and
  the screen shows source lines 256..375, not 0..119. With 0x80 and 0x40 likewise.

**What the scaler really is: a stream through the tiles**

The picture above is what comes out of something simpler. The scaler takes the tiles one after the other
as the region array lists them and does not know where a tile is vertically:

- A tile with y = 0 starts a new stream of lines (line 0, row 0, pos = 0, or 1024 for the second field);
  so does a frame. Every other tile continues the stream of the tile before it, whichever tile that was.
- A row is written when the stream has reached line k + 1 (k + 2 with the filter), at the x of the tile
  being worked on at that moment, at row (number of rows of the stream so far) & 1023.
- When a stream ends (the next tile has y = 0), at the x of its last tile: with the filter the last line
  counts once more as the line below itself; then the rows that need nothing beyond the last line are
  written. A row at the last line itself is only written when the fraction of pos is 0 (all 10 bits).
- Only with factor 0x400 is the scaler out of the way: every tile goes to its own place in any order.

Column by column this gives the picture one expects. Row by row it does not: the tiles of the top row all
land on rows 0.. of their columns, and from the second row on every tile continues 16 or 32 rows below
the previous one, to the right and wrapping at 1024 (scalertile pages 8, 9, 11, 12, 18, 19, 23..27; refsw2
reproduces each pixel). A region array in rows needs factor 0x400.

**A hardware bug at the end of the frame**

With the filter, the last row of the last tile of the region array is wrong on a console in 31 of its 32
pixels: where it uses the filtered last line, pixels 1..31 of the tile get the value of pixel 0 (the filter's
output stands still while the row is written). It moves with the last entry of the region array (bottom
right for KallistiOS; bottom left with the columns from the right), it is there with 0x401, the value
KallistiOS uses on a TV (row 478, x = 609..639), and it is not there without the filter. refsw2 does the
same.

Not measured: factor 0; what a stream that began before the first tile with y = 0 does with the field
offset in the next frame (the model starts every frame from nothing, which is what two frames in a row in an
order from the bottom show for factors 0x800 and 0x401).

## blendsel, blendsel2, blendpos: both select bits, the alpha that gets stuck, and where buffer 2 comes from (2026-11-16)

`blend` is 4096 of 4096 regions now (256 settings x 4 colour sets x 4 views, a fresh console run), and so are
blendsel (3072 of 3072) and blendsel2 (480 of 480). Three things were behind the 105 regions that differed.

**1. With source and destination both buffer 2 a polygon writes nothing, always.** Buffer 2 is left as it
was with all 64 pairs of factors, in every pixel (refsw2 already had that rule). What looked like
"sometimes it writes garbage" and "settings 236..255 are not reproducible" was the place: those settings
were the last 20 cells of the screen, the only ones followed by tiles with nothing in them (blendpos:
the same cells laid out backwards fail in the same places with the plain settings 0..19 in them; with
the empty cells filled, all 256 settings are right). `blend` fills its 44 spare cells now.

**2. The two accumulation buffers change places when a tile is written out.** What a tile finds in its
secondary buffer ("buffer 2") is the finished picture of the tile rendered before it, and what the
background plane overwrites is that tile's buffer 2. Seen in tiles that contain nothing but a polygon
reading buffer 2: they show the tile above, and when the polygon multiplies with the buffer's alpha the value
drops by one from tile to tile (ce, cd, cc: 255/256 of the picture before, not buffer 2 standing still). It
carries from the last tile of a frame to the first of the next; a region array entry that is not written
out (KallistiOS's first) leaves the buffers alone. refsw2 does this now (`NextTileAccumulationBuffers`).

A consequence that is not the same from run to run, and not modelled: the tile that was just rendered is
written out while the next one is rendered, from the buffer that the next tile has as its buffer 2. A tile
that reads buffer 2 right away (here: nothing in the tile but the view's pass) disturbs that. Three console
runs of blendpos with the same input (same checksums), page "variant 0, buffer 2's colour":

| | wrong pixels in each run | pixels that differ between two runs |
|---|---|---|
| the 20 tiles with content that an empty tile follows | 6664, 6961, 6635 | |
| the 44 empty tiles | 16761, 16889, 16713 | |
| the other 236 tiles | 0, 0, 0 | |
| the page | 23425, 23850, 23348 | 26631, 24307, 27619 |

About as many pixels differ between two runs as are wrong in one: the damage is somewhere else every time.
In a tile with content it starts between pixel 470 and 804 of the 1024 (rows 14 to 25, usually row 16) and
goes on to the end of the tile, about a third of the tile's pixels; the wrong pixels hold values from other
places of buffer 2 (the colours of the top squares in the bottom rows). In an empty tile it is in every
row. The pages where nothing reads buffer 2 (the framebuffer's colour) and the layout without empty tiles
(variant 2) are the same pixel for pixel in all three runs, and equal to refsw2. The blend unit itself is
not involved: all 256 settings are the same every time. That two readers of one memory collide is a
reading of this, not something measured; what is measured is the swap, where the damage is, and that it
moves. With a few passes before the first use of buffer 2 nothing happens (every tile of `blend`). blendpos
keeps showing it in its variants 0, 1 and 3 (3135 of 3204 regions).

**3. A bug of the blend unit: alpha that gets stuck.** Once a polygon with SrcSelect and DstSelect both
set, source instruction ONE or SRC_A and destination instruction ZERO has been drawn in a tile, every
polygon after it in that tile has its own alpha replaced by 0, unless it is 255, before it is blended:

| polygon after it | console |
|---|---|
| polygon -> framebuffer or buffer 2, ONE, ZERO | colour as usual; alpha stored as 0 (255 stays) |
| SRC_A, 1-SRC_A | destination untouched for alpha 254, 128, 1; replaced for 255 |
| ONE, ONE | colour added; destination alpha unchanged |
| buffer 2 -> framebuffer (SrcSelect) | as usual: its alpha comes from buffer 2 |
| without UseAlpha | as usual: its alpha is 255 |

One pixel of the first polygon is enough; polygons in between that change nothing do not clear it; the next
tile starts clean (blendsel2: 8 kinds of first polygon x 8 kinds of second, and one in the tile above).
The first polygon itself writes nothing (1.); this is its only effect. Other instructions with both bits
set, and ONE, ZERO with only one of the bits, do not do it. In `blend` it
showed as settings 193 and 196 reading alpha 0 in three squares of four: every square but the first drawn
(blendsel: draw the squares backwards and it is the other three; draw all test quads last and it is gone).
refsw2: `tspAlphaStuck`. Not measured: textured polygons (the alpha after the texture is replaced), what
else might clear it within a tile, the automatic sort.

## The TSP's numbers at their limits: zero, infinity, and a 1/w below zero (2026-11-17)

    python3 uvmeas/gen.py --mix whuge|wtiny|wneg|wspec|wzero|wzero2   # with wexp: 1/w at every size and of every kind
    python3 cmeas/gen.py --mix wext|wsum                   # colours at the same depths; the sums at the bottom of the range
    python3 mipmeas/gen.py --mix wext|wextd                # the mipmap level at such depths (wextd: DCalcCtrl 1)
    bash run2.sh detinf off hw=<ip>                        # the ISP's depth under an infinite determinant
    bash measure.sh uvmeas <name> --mix ...                # archived: uvmeas wexp1, whuge1, wtiny1, wneg1, wspec1, wzero1,
                                                           # wzero2; cmeas cwext1, cwsum1; mipmeas mwext1, mwextd1
    model/tspprobe [--cells] [--summary] [--misses N] [--only a-b] [range=0] fit/uvmeas_data/<set>.uvm

This replaces the section "The TSP at very small 1/w: there is no zero" (2026-11-16), which had one of its
rules wrong. Five sets of texture coordinates (3,900,000 readings) and one of colours (2,340,000 values) at
depths of 2^-149 to 2^127, below zero, zero, infinite and NaN are reproduced reading for reading by the model
(`pvr_tsp_plane.h`) and by refsw2-offline, together with every earlier set: uvmeas 29 sets, 22,080,053
readings; cmeas 2 sets; and covmeas, whose 8 open sets are right now (98 of 98). The same limits in the ISP's
depth setup make `huge` the console's in all 250 regions. A second round (the corners that the first left
unmeasured) added uvmeas wzero1 and wzero2, cmeas cwsum1 and mipmeas mwext1, and made `hugepos` exact:
uvmeas 31 sets, 23,640,053 readings; cmeas 3 sets; mipmeas 6 sets; all the console's.

**The numbers of the setup** have a sign, an exponent field of 8 bits and a mantissa, and two of the 256
exponents are not numbers:

- Field 0 is **zero**, whatever the mantissa: a denormal depth is zero. Field 255 is **infinity**: a NaN is one.
- **Product**: zero times anything is zero, also times infinity; infinity times anything else is infinity. A
  result of 2^128 or more is infinity, one below 2^-126 is zero. The sign is the product's in every case.
- **Sum**: infinity plus a number is that infinity; infinity minus infinity is +0. A result of 2^128 or more
  is infinity. A result below 2^-126 is **+0**, whatever its sign was (like an exact cancellation).
- **The last addition of a three term sum**, (t1 + t2) + t3: when one of the two operands is zero and the
  other is 2^-126 .. 2^-125, the zero is added as 2^-127 with its sign. -2.5 * 2^-127 + 0 comes out as
  -1.5 * 2^-127, which is below the range, so +0, and the slope made of it gets the other sign. In t1 + t2 a
  zero is a zero. (5 cells of wtiny1, 21 of cwext1, one of them telling the two additions apart; cwsum1,
  with the zero term in every place: 2,340,000 of 2,340,000, against 2,335,162 if a zero counted in t1 + t2
  too and 2,318,225 without the rule. Whether the upper limit is 2^-125 or 2^-124 is still not told.)
- Infinity is sticky and beats everything except zero and its opposite, so one overflowing product is
  enough: U * 1/w * (x1 y2 - x2 y1) reaching 2^128 makes the whole U/w plane an infinity or, with a second
  one of the other sign, a zero.

**The evaluator** (the plane at a pixel) has neither. It reads a coefficient's exponent as a signed 8 bit
number: a zero is **2^-127** with its sign, as found before, and an infinity (+128) wraps to **2^-128**.
With every product overflowing, both planes are -2^-128 - 2^-127 (x + y) on one winding (4875 of 4875
readings in a joint fit), and the texel is the same for any depth and any U. The grid of a tile counts these
two as the numbers they are read as. The value at the pixel becomes a float again with an exponent of at
least -128: below 2^-128 the mantissa is kept and the exponent stays. A value of -1 grid unit (every bit
set, no leading bit to find) gets the smallest exponent with a mantissa of -1 in its last place: -2^-143
(wzero1: four readings that -2^-128 got wrong; all that is told is that it is below 2^-130). A value of 0
stays 0.

**The divider** takes any denominator:

- **A 1/w below zero is not negated properly.** The leading bit stays and the 15 bits under it are negated:
  a mantissa m (32768 .. 65535) is read as 32768 + (-m mod 32768). So -v between two powers of two is seen
  as 3 * 2^p - v (-1.25 as 1.75, -1.75 as 1.25) and a power of two as itself. The sign is dropped: the
  quotient has the sign of the numerator alone. wneg1: 780,000 of 780,000 readings with 1/w of -2^-6 .. -2^8
  and with both signs inside one triangle.
- A quotient below zero stays below zero however small: its texel is the one before 0.
- **A numerator of 0 gives 0**, whatever the denominator (`hugepos` triangle c: the one pixel where both
  planes are exactly 0 is black, not white). **A denominator of 0** under a numerator that is not is read as
  2^-128, like everything else without a leading bit (wzero2: 1/w falls through exactly 0 on a diagonal of
  each cell, 32,784 readings there; 2^-127 and 2^-129 each get 23,000 fewer).
- There is no "1/w that is not above zero gives the largest result" (the rule of 2026-11-16). What looked
  like it: with a zero, denormal, infinite or NaN depth both planes are the same zeros (or infinities), the
  quotient is one unit under 1, and that is texel 1023, or white.

**Colours are fractions.** In the planes a colour is its 8 bit value times 2^-8. With ordinary depths that
changes nothing; at the limits it decides what overflows (a colour of 0x86 at 1/w = 2^110 is still 0x86) and
makes the quotient of two equal planes white. cwext1: 2,333,331 of 2,340,000 with 2^-8 against 1,688,587 with
2^-7 and 2,124,860 with 2^-9 (and all 2,340,000 with the rule of the last addition).

**A pixel with a depth below zero is drawn.** `huge` said it is not, "even with compare ALWAYS": its quads
are untextured and white, the colour's quotient has the sign of colour / w, which is negative, and the
result is black. refsw2 no longer skips such pixels, which is what the 8 open covmeas sets needed.

**The ISP's depth setup has the same limits** (`pvr_isp_depth.h`, `DepthConfig::range`): field 0 zero, field
255 infinity, the same sums and products, and an infinite depth at a tile's first pixel is stored as an
infinity. This was not measured number by number. It was tried because the 22 regions of `huge` that were
left were depth comparisons with 2^127, the largest number, infinities and NaNs, in which a console often
draws one triangle of a quad and not the other; with these rules the two triangles' sums come to infinities
of opposite sign, and `huge` matches in 250 of 250 regions. An infinite determinant (corners at 2^64 and
beyond) is taken to give a depth of 0: a guess, which one pixel of `modvol` needs and nothing contradicts.

**The reciprocal of the determinant has the range too.** Of a determinant above 2^126 it is below 2^-126, so
zero, and every coefficient of the TSP's planes is a zero with the sign of its sum times the determinant's.
That is the `hugepos` limit that was not modelled: triangle c (determinant -2^127.4) is black, the quotient of
two planes below zero; b (+2^127.7) is white exactly where x <= y, where -2^-127 x + 2^-127 y + 2^-127 is
above zero. `hugepos` (triangle c, archived): 16 of 16 pages. Triangles a and b, and the three together in
the translucent list, were run on the console again: b exact, a and the three together exact up to 2^124;
at 2^125, where a coordinate difference itself overflows in the edge unit, the console draws 4 pixels at
the origin and refsw2 one (neither saturating that difference nor zeroing it gives the 4).

**The mipmap level** (mipmeas mwext1: 2400 quads with flat 1/w of 2^-126 .. 2^127, below zero, zero,
infinite and NaN, several ratios, biases and both passes; 4,147,200 of 4,147,200 colour values, the five
earlier sets unchanged). The level's numerators, a_u c_w - c_u a_w and the like, are made of the planes'
sums before their division by the determinant, and the limits apply to those products: with a flat 1/w = z
on a 32x32 quad a product is about z^2 2^15.6, so it is an infinity from z = 2^57 on and zero below
z = 2^-60. Infinity minus infinity is zero; zero times infinity is zero. Then:

- K, the longer length times the bias, gets the texture's size added to its exponent field without a look
  at what the field means: a zero (field 0) becomes 2^(-127 + size), an infinity (field 255) wraps to
  2^(-128 + size). Below 2^-60 the level climbs by two for every halving of z, up to the smallest map;
  above 2^56 it is the largest map with no blending; near the top w * w is small again and the level climbs.
- w * w (8 bits) does not leave the range: its exponent stops at 127 and the mantissa is kept.
- The sign of 1/w does not matter: w is the divider's reciprocal of the denominator as it reads it
  (DenominatorSeen), and its sign is dropped.
- With DCalcCtrl 1 the numerators are taken at the first vertex (u1 z1, v1 z1, z1), and for the limits the
  undivided planes are evaluated there, a x1 + b y1 + c, in the setup's arithmetic: a flat 1/w overflows from
  2^57 as with DCalcCtrl 0, an infinite or NaN z1 gives the levels it gives there, and a triangle whose 1/w
  sums are all zero has a zero numerator whatever z1 is (mipmeas mwextd1: 4,147,200 of 4,147,200; with the
  vertex's own z1 times the determinant instead, 4,146,600).

**The level is a polygon's constant times w^2, and DCalcCtrl says where the constant is taken** (mipmeas
mpos1: the same perspective quad in all 300 cells of a page). For u = (aX + bY + c) / (pX + qY + r) the
derivative is du/dX = ((aq - bp) Y + (ar - cp)) / (pX + qY + r)^2: the numerator is linear in the other
coordinate, the denominator is 1/w^2, so w^2 per pixel. The hardware freezes the numerators once per
polygon, in the setup, and per pixel only multiplies by w^2 (w being the reciprocal the TSP has for the
perspective division anyway): two 8 bit multiplies and a leading-one search. With DCalcCtrl 0 the
numerators are taken at the screen's origin (ar - cp), so a quad with perspective gets another level
wherever it is put: 47 different pictures in 300 identical quads with 1/w falling along x, 48 along y, 20
along both; the x numerator moves with the quad's row and the y one with its column, and the level
follows the larger. With DCalcCtrl 1 (at the first vertex) all 300 are the same, as they are with a flat
1/w either way. refsw2-offline: 2400 of 2400 cells.

**The ISP's depth under an infinite determinant** (`detinf`: triangle a of `hugepos` at 2^64 .. 2^100 with
several depths, then probes of many depths with compare GREATER and LESS). refsw2's guess, a depth of 0, gives
the console's picture for 478 of 480 probes: the stored depth is below 2^-125 (every probe from there up
passes GREATER, none from 2^-100 down passes LESS). Probes of 2^-126 and below are white when drawn, like the
triangle, so they cannot say more, except two: with corner depths 1, 0.5, 0.25 the probes of 2^-126 and one
unit above it get through on the console, and with a flat 0.5 they do not. So what is stored below 2^-125
depends on the depths; not known.

How it was found, in case it has to be done again: wexp1 has the exponent tied to the place on the screen,
which hides what depends on what; the new mixes draw exponent, size of U, winding and place independently.
On the winding where the planes fall with x + y, a denominator below zero makes the reading depend strongly
on both planes (quotient -v / (3 * 2^p - v)), and fitting the six coefficients to a cell's 325 readings
gives them to a fraction of 2^-127. That is how 2^-128 for an infinity and the sign rules were read off.

What changed in refsw2 (uncommitted): `pvr_tsp_plane.h` (the range in `Plane::Setup`, `Plane::Value`,
`DenominatorSeen`, `Divide`), `pvr_isp_depth.h` (the range in `DepthTile::Setup`), `refsw_tile.h` (colours
times 2^-8, no special case for a 1/w that is not positive, a tiny negative quotient kept negative),
`refsw_tile.cpp` (pixels with a negative depth are drawn). `PlaneConfig::range = 0` and
`DepthConfig::range = 0` give the arithmetic as it was.

| | before | now |
|---|---|---|
| uvmeas wexp1 | 607,574 of 780,000 | 780,000 |
| uvmeas whuge1, wtiny1, wneg1, wspec1 | 323,400 / 481,958 / 95,721 / 296,023 | 780,000 each |
| cmeas cwext1 | 1,468,125 of 2,340,000 | 2,340,000 |
| covmeas | 90 of 98 sets | 98 |
| huge | 195 of 250 regions | 250 (228 with the TSP's rules alone) |
| hugepos (triangle c) | 9 of 16 pages | 16 |
| uvmeas wzero1, wzero2 | 779,996 / 757,439 of 780,000 | 780,000 each |
| cmeas cwsum1 | 2,318,225 of 2,340,000 without the last-addition rule | 2,340,000 |
| mipmeas mwext1 | 2,127,698 of 4,147,200 | 4,147,200 |
| mipmeas mwextd1 (DCalcCtrl 1) | 4,016,968 of 4,147,200 | 4,147,200 |
| detinf | | 478 of 480 probes |
| mipmeas mpos1 (position) | | 4,147,200 |

Still not known: whether the limit of the last-addition rule is 2^-125 or 2^-124 (probably not observable:
an operand of 2^-125 .. 2^-124 keeps its sign either way, and a coefficient made of it is below the range
unless the triangle is smaller than two pixels); an infinite coordinate; what the ISP stores below 2^-125 for
an infinite determinant (see `detinf`); the edge unit when a coordinate difference overflows (the 3 pixels of
`hugepos` a at 2^125); the ISP's stored depth as a number at these depths (what is known is which pixels
pass, from `huge` and `detinf`).

What refsw2-offline does not reproduce after this: `accbuf` (the two tile buffers are not modelled),
`blendpos` (69 regions that differ from run to run on the console), `modvol` (5 regions of two-volume
polygons), `detinf` (2 probes, above) and one depth bracket of zmeas `zero6`.

## The generators are C++ programs on the refsw2 headers (2026-11-16)

covmeas, zmeas, uvspan and ops place their cases by what the hardware will do with them. They took that
from a Python copy of the model, `ops/model.py`; that copy is gone. Each generator is now `<test>/gen.cpp`
and includes the soft floating point headers of refsw2 directly, through `model/gen_common.h` (which adds a
JSON reader and writer, a random number generator that is the same with every compiler, and the model asked
about one pixel: inside or not, the stored depth, a TSP plane, the TSP's division). No Python calls C++.

    bash model/build.sh [test...]          # the probes, and <test>/gen and <test>/read where there is a .cpp
    <test>/gen [the arguments gen.py had]  # cases.h and cases.json in the test's folder
    <test>/gen --regen <set>.cases.json <folder>      # the tables of an archived set (regen.py, run2.sh <test>@<set>)
    zmeas/gen --check <set>.cases.json     # does the model still give the numbers the set was made with? (also ops)
    zmeas/read <folder> [hw|off]           # replaces zmeas/read.py, which needed the model for the probe's depths

`measure.sh`, `archive_check.sh` and `regen.py` use these where a test has them; uvmeas, wmeas, cmeas and
mipmeas never used the model and keep their gen.py. `model/GENPORT.md` is the guide the ports followed. The
Python originals are in `fit/python_generators_before_cpp.tar.gz`.

What was checked, for each: the tables written from every archived set are the Python's from the second
line on (zmeas 7 of 7, covmeas 80 of 80, uvspan 2 of 2, ops the one set); `archive_check.sh` prints what it
printed before; and new sets from the C++ generator were measured on the console:

| | new sets on the console | refsw2-offline against the console |
|---|---|---|
| zmeas | 4, one of each mix (cgen21, cfirst24, czero22, cscene23) | every bracket; all 1200 cases exact and equal to the model |
| covmeas | 18: every mix as triangles, lattice and lean also as strips, sprites, modifier volumes, a depth probe | every pixel of all 18 |
| uvspan | 2, one of each list (cspan31, cspan32tr) | every reading (472,135 and 468,570) |
| ops | the test's set, made again | 409 of 409 regions |

The C++ model gives the Python model's number on everything compared: every predicted and ladder depth of
the zmeas sets (the older sets hold predictions of earlier versions of the Python model; today's Python and
the C++ differ from them in the same 43, 115 and 20 places), all 526 expectations of the old ops set, and the
248,069 edge values the covmeas lean search evaluated.

Changed on the way:

- A seed gives other cases than it did (numpy's random numbers are not reproduced). Archived sets are data
  and run as before. The two zmeas scenes had their polygons made again from the seed by `write()`; they now
  carry them in their cases.json ("tris"; the files as they were are kept as `.before_tris`).
- **ops tests the fields of the C++ model**, not the operations of the Python one: every field of
  `DepthConfig`, `PlaneConfig` and `EdgeConfig` two steps either way, 122 alternatives. On the console the
  model is right on all 409 cases and no alternative gets one; 94 alternatives have cases that tell them
  apart. The 28 without: `DepthConfig::coefBits` (nothing it cuts survives the 20 bits kept afterwards, so
  its 24 is not a measured number), `PlaneConfig::reach` (not told from "no limit" by these triangles),
  the stored bits below 19, and what needs covmeas's or hugepos's kind of triangle (edge reach, tie
  shift, the last column, overflow). The earlier section "ops: one operation at a time" describes the old
  alternatives.
- **The ops cover cases were not next to an edge.** gen.py moved the edge away from the probe pixel instead
  of onto it (a sign), leaving it half a pixel off on average; only 4 of the edge alternatives got their ten
  cases. Corrected: the edge passes within 2^-6 to 2^-21 pixel, and 36 of 53 do.

## timemeas: what the ISP, the TSP and the write-out cost in time (2026-11-16)

    bash run2.sh timemeas hw=<ip>             # about 30 seconds; the timing is in the console's log
    tr -cd '[:print:]\n' < /tmp/pvr_tests/timemeas/build/hw.log > fit/timemeas_logs/timemeas_run<n>.log
    python3 timemeas/analyze.py > timemeas/tables.txt

**The report is [timemeas/REPORT.md](timemeas/REPORT.md)**, the raw tables are timemeas/tables.txt. In
short, in clocks of the core (10 ns):

- **Method.** One tile in the region array, interrupts and display off, the TSP's end-of-render bit (the
  end of the write-out) timed from STARTRENDER with the CPU's cycle counter. The ISP's bit is not used: it
  is not the end of the ISP's work. ISP costs are objects behind the background polygon against the
  background alone; TSP costs are a scene against its twin with the same polygons behind the background.
- **The background polygon** is shaded by the TSP in every region array entry, also one that draws nothing
  and is not written out (KallistiOS's first): 1035 clocks each, 1.01 a pixel.
- **ISP**: 16 per triangle (a strip of two 34, a sprite 19, culled 15), whatever the format, the depth
  result or the place; one more per line and triangle beyond 16 lines; 20 to 31 per list that has a pointer.
- **TSP**: 1.01 per pixel (bilinear 1.11, mipmapped bilinear 1.37, texels 8 apart 3.27; one clock more, 2.00
  to 2.07, for a palette with ARGB8888 entries, for YUV422 bilinear and for bilinear without twiddling; a
  palette with 16 bit entries costs nothing), but
  at least 39 per seen triangle untextured, 48 textured, 64 with offset colour, and at least about 4.7 / 6 /
  8 per run of pixels; runs cost up to twice that when more than about 12 different triangles are on a line.
- **Write-out**: about 1045 with a 16 bit framebuffer, 1105 with 888, 1357 with 32 bits, also when the clip
  removes every pixel. A tile alone with nothing in it and pointers to five lists: 2695 (16 bit) or 3005
  (0888).
- **Modifier volumes** (the same scene with a volume closed as "inside" and as "outside", framebuffer read
  back). In the **punch through** list the pixels inside an opaque modifier volume are shaded twice: the
  first volume, then the second on top with its own blending (ONE, ONE: 0x406080 + 0x202020 = 0x6080a0),
  also with cheap shadow. There inside minus outside is the TSP's work alone, and it agrees with the twin
  method to 0.1 % (39.0 per untextured triangle, 48.0 textured; one more pass over a block: 81 + 1.0 a
  pixel). In the **opaque** list there is one pass: the second volume's parameters take the first's place
  (a blending second volume blends with the buffer's stale content), and a volume costs ISP time only.
  **Translucent** modifier volumes work in a tile that is sorted per pixel (cheap shadow and two volumes,
  one pass, an opaque polygon under it untouched) and are ignored in a pre-sorted tile, which is what
  these tests render by default. A pre-sorted tile with an empty opaque list and a translucent modifier
  volume skipped its background in some runs, depending on what was rendered before (not understood).
  Otherwise the background is drawn with or without an opaque list (checked by changing its colour).
- **The keep Z bit** of a region array entry skips the Z clear and the background polygon: the background
  costs a tile 1177 (1035 of shading, 142 before it), a tile with nothing to do costs 335 plus its
  write-out (1355 with 0888, measured by itself), and an entry that draws nothing costs 17 with keep Z
  against 899 without. A second entry of a tile with keep Z keeps the depth, shades only what it draws,
  and adds to the first entry's picture only if the first was not written out.
- Stages of consecutive entries overlap: a further tile of background costs 1040 (16 bit: the TSP) or 1357
  (32 bit: the write-out), and the ISP's work on a tile is free while the TSP shades the entry before.

## accbuf: the TSP's two tile buffers (2026-11-16)

    bash run2.sh accbuf off hw=<ip>          # 9 pages; on a console also the timing lines "T end", "T stall"
    python3 accbuf/analyze.py hw             # what every cell shows (colours, counts, bounding boxes)

Every page is one render with a region array of its own, one entry per cell, so that a buffer's old content
shows up in a later cell. The tools are the entry's keep Z bit (no Z clear, no background: what the tile does
not draw stays as the buffer had it), the not-written bit, and polygons with the TSP word's select bits: SEC
WRITE (destination select: the colour goes to the secondary buffer) and SEC COPY (source select, ONE / ZERO:
the secondary buffer's pixels come to the tile). Two console runs; logs in `fit/accbuf_logs`.

- **There are two buffers.** Ten keep Z tiles, each drawing one stripe: every tile shows the stripes of the
  tiles two, four, six before it, on the grey of the ordinary tile that was in that buffer last (page 2).
- **The other buffer is the secondary accumulation buffer.** A stripe that a tile writes with the destination
  select bit is in the picture of the next keep Z tile, which is shaded into that buffer, and not in the tile's
  own; SEC COPY brings what SEC WRITE wrote just before (page 5). The select bits work in the opaque list as
  in the translucent one (page 1).
- **A polygon that writes into the secondary buffer waits until the tile before is written out.** A tile of
  bands is never damaged by a SEC WRITE in the next tile, whether a column or the whole tile, and however
  long the next tile's ISP is kept busy first (pages 6 and 8: 58 cells, all whole, the same in both runs).
  And it costs the time: a whole tile of SEC WRITE takes 1025 clocks longer than a whole tile of ordinary
  pixels (0888 and 565 alike), a column of 4x32 takes 150 to 190 longer. Shading into the tile's own buffer
  runs beside the write-out; shading into the other one cannot.
- **A polygon that reads the secondary buffer does not wait**, and the picture being written out is damaged:
  3 to 5 of its first lines get pixels of other lines (page 7: 29 of 29 cells, different in the two runs;
  the timing is the same as without the read).
- **The write-out of a tile starts when the TSP starts on the next tile**; that is when the buffers change
  places. On page 7 the damage is in the first lines whether the next tile's shading starts at once or 3000
  clocks later.
- **A tile that is not written out does not change the buffer**: the next tile is shaded into the same one
  (page 4).
- **A keep Z tile that shades nothing collides with the write-out before it.** Its end comes at once (no
  background, nothing to shade), while the tile before is still being written out, and unlike a tile that
  shades it does not wait. Such a tile is not written to the framebuffer at all; two of them in a row and
  the render never ends, until the core is reset ("T end 2, 5, 6"); one of them followed by other tiles
  costs 2390 clocks more than its place in the row explains, and a tile before it comes out with the
  other buffer's picture (page 3, cells 6 and 12; page 0, H and K). That it is a collision shows when
  the empty tiles are held back with n unseen quads, which keep their ISP busy for about 34 clocks each
  ("T flush"; a write-out takes 1355 clocks with the 0888 framebuffer and 1045 with 565):

  | | 0888 | 565 |
  |---|---|---|
  | two empty tiles last in the region array, n quads each: the render ends from | n = 36 | n = 32 |
  | ... and every picture is right from | n = 56 | |
  | one empty tile with n quads, then an ordinary tile: no extra time, pictures right, from | n = 96 (not yet at 64) | n = 64 (not yet at 56) |
  | an empty tile, a second with n quads, then an ordinary tile: the render ends from | n = 96 (not yet at 64) | n = 64 (not yet at 56) |

  The first threshold is one write-out's time, the others about two, and all of them are lower with the
  shorter write-out. Held back long enough, empty keep Z tiles do no harm (and are still not written).
- refsw2 agrees in 199 of 375 cells: it copies the finished tile to a second buffer at the write-out instead
  of having two buffers that change places, and it shades kept tags again in a keep Z tile.

## yuvbil: YUV422 texels and bilinear filtering (2026-11-16)

    bash run2.sh yuvbil off hw=<ip>          # 3 pages; refsw2-offline = console in 164 of 164 regions

- **The blend is made of RGB values.** Each of the four texels is converted to RGB, and clamped, and then
  the four are blended as for any other texture. The conversion is linear except where it clamps, so the
  test uses pairs of colours of which one clamps (a red of 302 against grey: the midpoint is 191 if the
  texels are converted first and 215 if the YUV values are blended first). The console shows the first, to
  the last bit, in 14 pairs of colours and 6 textures of unlike texels, twiddled and not.
- **Which two texels share U and V goes by x, also in a twiddled texture.** A texel is Y and one chroma
  byte; the texel at an even x has U and its neighbour at x + 1 in the same row has V. In a twiddled texture
  those two are not next to each other in memory (two apart in the index). refsw2 paired by memory order
  and had the bits of the index mixed up, so only a YUV texture of one colour came out right (which is all
  texfmt has); corrected in `DecodeTextel` (refsw_tile.cpp), and texfmt, mip and sample are unchanged.

## texfmt, mip, sample: every way a texture is stored, every format, every filter (2026-11-16)

    bash run2.sh texfmt mip sample off hw=<ip>      # 46 + 20 + 50 pages

The three tests had one kind of texture each (twiddled RGB565, point and bilinear; YUV422 only as one
colour). They now share `common/texgen.h`, which builds a texture's memory from a function that gives the
texels, for every layout: twiddled, row by row, row by row with a stride, mipmapped, VQ, VQ mipmapped, with
16, 8 or 4 bits a texel. refsw2-offline matches the console in every region of all three (3864, 4423 and
7815), after these were found on the console and put into refsw2 (refsw_tile.cpp, not committed):

- **A strided texture** (scan order and stride select set) **ignores the flip bits**: it repeats or clamps,
  but is never mirrored (sample, "stride 32" variants).
- **A strided texture filtered bilinearly is widened differently**: 565, 1555 and 4444 texels get zeroes for
  low bits (R of 565: the 5 bits shifted left by 3) where every other texture, and this one point sampled,
  repeats the top bits there. The alpha of 4444 too; the one alpha bit of 1555 is 0 or 255. YUV422 is not
  affected (sample, texfmt "stride 32, bilinear").
- **With the scan order bit set, the mipmap and VQ bits do nothing to where a texel is fetched**: the texture
  is read row by row, one level, uncompressed (mip page 17: the texture memory holds its own index in every
  word, so the picture shows which word was read, for all 16 settings of the four bits). With the scan order
  bit clear, the stride select bit does nothing.
- **The mipmap bit still fades the trilinear passes** of such a texture by the level's fraction, although
  there are no levels to choose from (mip, "scan order bit set" variants).
- **Trilinear on a texture without mipmaps is bilinear**, at full strength in both passes (mip, "565 without
  mipmaps"); refsw2 drew the second pass black.
- YUV422: the two texels that share U and V are neighbours in x, also in a twiddled texture (see yuvbil).
- **A mipmapped texture whose polygon declares a V size other than the U size** is as tall as it is wide
  (the U size), but with bilinear filtering the V coordinate is made with the declared V size, half texel
  included, and then scaled to the real height: declared 16x8, the footprint sits a whole row higher than
  declared 16x16, not half a row (sample "declared" variants; mip page 19 for ratios up to 8, also VQ).
- **A VQ texture of palette indices**: a code book entry is 64 bits of texels in twiddled order whatever
  the texel size, so 8 texels of 8 bits (2 wide, 4 tall) or 4x4 texels of 4 bits (sample, "palette VQ").
- Bump maps: see the next section.

What is covered now:

- **sample** (50 pages): sampling positions (point and bilinear, shifted in eighths of a pixel, magnified,
  1:1 and at half size), all 256 bilinear weights along u and v, and the 16 wrap modes point sampled and
  bilinear, for 30 kinds of texture: 565 twiddled, row by row, strided, mipmapped, VQ, VQ mipmapped; 1555
  and 4444 twiddled and strided; YUV422 twiddled, row by row, strided and VQ; palette 8 and 4 bpp, twiddled
  and VQ; bump maps twiddled, VQ and row by row (the last point sampled only); 565 16x8 and 8x16 twiddled,
  row by row and VQ; 565 mipmapped and VQ mipmapped with a declared V size that is not the U size.
- **mip** (20 pages): the first five pages as before; the level choice and the trilinear blend (point,
  trilinear pass 1 and 2, bilinear, five biases, 18 quad sizes) for VQ mipmapped, 1555, 4444, YUV422, palette
  8 and 4 bpp, a mipmapped texture with the scan order (and stride) bits set, and a texture without mipmaps;
  the page that shows which word of memory is read; and two pages of textures that are not square, every
  texel its own colour, a texel per pixel: 32x8, 8x32, 64x8, 8x64, 32x16, 16x32 twiddled, row by row, VQ and
  (declared) mipmapped, and four VQ mipmapped ones, point sampled and bilinear.
- **texfmt** (46 pages): how a texel's bits become 8 bits a channel (every channel of 565, 1555, 4444 swept,
  their alpha, YUV422's Y, U and V swept) for ten kinds: twiddled, row by row, strided, mipmapped and VQ, each
  point sampled and bilinear; and the four palette entry formats with 8 and 4 bpp textures, point and bilinear.

Not in them: textures larger than 64 texels a side with VQ or mipmaps; the palette entry formats other than
ARGB8888 through a VQ texture.

## bumpfn, bumpbil, bumphang: bump maps (2026-11-17)

    bash run2.sh bumpfn bumpbil off hw=<ip>         # 7 + 7 pages; refsw2-offline = console in 952 and 1792 regions
    bash run2.sh bumphang hw=<ip>; python3 bumphang/analyze.py     # stops the console's render 17 times, ~1 min

The old `bump` test draws its result with (ONE, ZERO), and a bump texel's result is its alpha: the pages are
white on the console and in refsw2, whatever the function. These three show the intensity through
(SRC_ALPHA, ZERO) on a white base colour.

**The function** (bumpfn: a 256x256 bump map with S = y, R = x, one texel per pixel, under 14 lights; a
strip of a palette texture with alpha = x through the same blend gives the way back from picture to
intensity). With the light K1, K2, K3, Q in the offset colour's bytes A, R, G, B:

    a = |R - Q|                       the magnitude of the difference, not its low 8 bits
    x = a & 63, quadrant = a >> 6
    across = cos[4 * x] in quadrants 0 and 2, cos[4 * (63 - x)] in quadrants 1 and 3
    I = K1 + ((K2 * sin[S]) >> 8) +- ((((K3 * cos[S]) >> 8) * across) >> 8)     minus in quadrants 1 and 2
    clamped to 0..255; the texel's colour is white and its alpha is I

- Every product drops its low 8 bits before the next step: K3 = 255 at S = 0, R = Q gives 253, not 255.
- `sin` and `cos` are two tables of 256 bytes (`bumpfn/tables.txt`; `BM_HW_SIN`, `BM_HW_COS` in
  refsw_tile.cpp). They are not a sine and a cosine: sin[64] is 104 where 256 sin(22.5 deg) is 98, sin[128]
  is 177 (181), and the cosine is not the sine read backwards (cos[64] = 231, sin[192] = 233). Each is a
  line per 8 entries, (b + m * (S & 7)) >> 3, with m = (32 - i) >> 1 in the sine's segment i and
  -((i + 1) >> 1) in the cosine's; the 32 bases b of each were read from the console and follow no rule I
  found. sin[0] may be 0 or 1 and cos[255] 0, 1 or 2: no light can show which.
- The angle is not taken modulo a full turn: R - Q below zero is negated. The low 8 bits of a negative
  difference -d would land in the mirrored quadrant and read the table four entries away from where d
  reads it (R below Q was wrong in up to a fifth of the texels with that rule).
- refsw2 had 256 * true sine and cosine with a division by 127 squared: off by up to 18 of 255.

**Bilinear filtering** (bumpbil: two-texel textures, the weight swept over a cell; sample's bump variants).
The two bytes of the four texels are filtered and the function is applied to the result. S is filtered
like a colour channel. **R is taken round the circle the shorter way**, in the same two steps (along u in
each row, then between the rows): the step starts at the end from which the other is at most half a turn
upward (exactly half a turn: from the larger value) and adds (distance * weight) >> 5; the weight is the 5
bit weight when the step starts at the first texel and its complement, 31 - weight, when it starts at the
second, never 32. Halfway from 210 to 0 is 233. From 0 to 64 the 32 pixels of a cell read 0, 2 .. 62; from
64 to 0 they read 62, 60 .. 0, not 64, 62 .. 2.

**A bump map that is not twiddled, filtered bilinearly, stops the render** (bumphang). The console never
signals the end of the render; tiles after the one where it stopped keep the earlier picture. The harness
now waits 2 s, resets the core (SOFTRESET 2, then 0) and goes on, and the log says "the render did not end".

- Only that combination: twiddled and VQ bump maps filtered bilinearly, the same texture point sampled, and
  other formats that are not twiddled all render to the end.
- It counts quads of that kind as tiles shade them and stops at the 16th (translucent list) or the 8th
  (opaque and punch-through lists): one quad per tile stops at tile 15, four per tile at tile 3, a quad in
  every third tile at tile 45. A quad counts once in every tile it covers.
- Size, magnification (8x to half size), texel values and light make no difference.
- The count carries over to the next render (ten quads a render: the second render stops at its sixth) and
  starts again after the core reset.
- Why is not known. sample leaves that variant's bilinear cells empty; refsw2 draws such pages to the end.

## Still not covered

Translucent modifier volumes and two-volume textured polygons; render-to-texture and its 1555 /
4444 formats; bilinear weights versus coordinate precision; the opaque and
punch-through lists for fog and bump (bump: only that they stop the render sooner, see bumphang); everything on more than one console.

