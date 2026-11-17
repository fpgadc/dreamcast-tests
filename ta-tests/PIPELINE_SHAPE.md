# The tile accelerator's pipeline: a best guess

Written 2026-11-17 from the three TA tests in this folder (ta-tests, ta-probe, ta-perf) and from refta (nulldc3
`libswirl/rend/refsw2/refta.cpp`), which reproduces every console dump it was checked against except those listed in
the README ("Not in refta"). Nothing here was seen inside the chip. It is the cheapest hardware that gives the
behaviour seen from outside. Each claim carries a tag:

- **[M] measured**: a console dump or timing shows it directly (case names in brackets, ta-probe's unless marked)
- **[I] inferred**: the simplest structure that explains several measured behaviours at once
- **[G] guess**: fits the data, but other structures would too

Times are given in PVR clocks (100 MHz, 10 ns) next to ns. ta-perf counted 5 ns CPU cycles, with DMA input unless
said otherwise.


## Overview

```
 SH4 bus: store queues, or channel-2 DMA (at best one 32 byte block per 54 ns)
   |
   v
 [1] input FIFO, 8 blocks of 32 bytes (SB_TFREM). Full: the bus write stalls, with no timeout
   |
   v
 [2] block decoder + control FSM: PCW, list state, header registers, 2-block assembly
   |        \--> registers: user clip, face colours, Strip_Len, volume box, last header
   v
 [3] vertex converter: float colour -> 8 bit, intensity x face colour; coordinates pass as sent
   |
   v
 [4] strip assembler: up to 8 vertices; split by Strip_Len; re-emits header + 2 vertices
   |                                   \
   | parameter words                    \ x, y of each triangle
   v                                     v
 [5] parameter FIFO + writer          [6] bounds: tiles of each triangle, union, volume box,
     write address W,                     user clip, off-grid cull, single tile bit
     size counter A,                          |
     commit pointer TA_ISP_CURRENT            v
   |   <--- commit or discard ---------- [7] tile walker + tile state RAM (608 words visible,
   |        size, single tile bit           + hidden open-array state), join rule, blocks
   v                                          |
 VRAM writes                             [8] object list write queue (32, marks 31 / 16)
   \________________ shared VRAM write path? ____________/

 [9] end-of-list pass: every tile of the grid, open arrays and end words, queue drained, interrupt
```

refta has the same split. [2] is `Block`, [3] `PolyVertex`, [4] `StripVertex`, the first half of `Commit` and the
`param*` functions are [5], the second half of `Commit` is [6], `Append` and `Allocate` are [7], `OlWrite` and
`Drain` are [8], and `EndList` is [9].


## [1] Input FIFO

- 8 blocks of 32 bytes: SB_TFREM counts free 32 byte units, and 8 means empty [M].
- No overflow handling. A TA that stops taking blocks stalls the store-queue write once the FIFO is full, and every
  later PVR access waits behind it, with no timeout. A console stuck like this needs a restart [M lock_type3,
  lock_type6, the first err_sprite_in_strip run].
- Intake is at least one block per 54 ns (5.4 clocks): with DMA, a stream of headers alone ends as soon as the DMA
  does [M ta-perf hdr]. The TA's own limit for cheap blocks is not known, because the DMA was slower.


## [2] Block decoder and control FSM

- Each block's first word is decoded (para type in bits 31-29). For a 64-byte parameter, the 16 words are gathered
  first and the second block is taken as data, whatever its PCW says: a header or an end of list there is not seen
  [M listless_*, err_vtx_half, err_hdr_half].
- List state: the first header after a list start sets the list type. Headers of another list type then join the
  open list, because the type field is not looked at again [M err_list_switch]. Headers of list type 5-7 are not
  taken at all [M list5-7, err_list_types]. An end of list with no list open opens the opaque list and ends it
  [M init_one, init_hdr: their second end of list].
- Inside a strip only vertices and user clips are acted on. A header or an end of list there is ignored [M
  err_hdr_in_strip, err_eol_in_strip, err_clip_in_strip]. **[I]** The FSM has a "strip open" state that masks every
  para type except vertex and clip.
- Para type 2 is "object list set" [M olset_*]. Types 3 and 6 stop the TA until SOFTRESET [M]. **[G]** 3 and 6
  decode into a state that waits for a handshake that never comes, maybe a reserved path next to type 2's.
- With no header since the list began (or since TA_LIST_INIT, or with no list open), a vertex is read as a modifier
  volume vertex is: two blocks, nine words, three x, y, z. It becomes a triangle with the last header's ISP word
  [M err_vertex_first, listless_*, i8_zero, i8_done]. **[I]** The "header taken" flag selects the vertex format,
  and without it the decode falls back to the modifier volume format. TA_LIST_INIT clears that flag but not the
  strip assembler [M i8_*].
- A sprite header in the middle of a strip changes the format at once. The strip's next vertex block is then decoded
  with the sprite's field selects: that block's first word lands in every field. The block after it is swallowed as
  the second half of a 64-byte sprite vertex, and then the sprite header applies [M sprite_hdr_mid, sprite_mid_*].
  **[I]** The field mux select comes straight from the live format register, and the strip assembler is not told.
- Kept across lists: Strip_Len (SOFTRESET makes it 3), the user clip rectangle (SOFTRESET makes it tile (0, 0)), the
  two face colours (kept even across SOFTRESET), and the last header's ISP word, shadow and two-volume bits for
  listless vertices [M strip_len_*, uclip_*, int2_*, err_vertex_first].


## [3] Vertex converter

- Float colour to 8 bits: floor(256 c) - 1, clamped. Negative (also -0, -inf, -NaN) gives 0; 1 or more, +inf or
  +NaN gives 255. Intensity: floor(256 i), with NaN as 1.0. Face colour times intensity, each channel `>> 8`
  [M ta-tests float sweeps, the whole 256 x 771 intensity table]. **[I]** All of this is exponent and mantissa logic
  on the float's bits (a shifter and a compare), with no floating-point adder.
- Coordinates and depths are written as sent. They are only taken apart for binning [M].
- A sprite's two colours are the face colours that intensity mode 2 uses afterwards [M ta-tests fuzz_002].
  **[I]** One register pair holds both.
- Cost: the input format does not matter, only the parameter words written do. Float colours cost no more than
  packed ones [M ta-perf]. The only exception: textured with float colours and offset is 27 ns over the line.
  **[I]** The converter keeps up with the input, so it is never the bottleneck.


## [4] Strip assembler

- Strip_Len gives at most 3, 4, 6 or 8 vertices a polygon. A longer strip continues as a new polygon with the last
  two vertices, in the order (v3, v2, v4) after an odd triangle. The header is written again for each polygon
  [M ta-tests strip_*, s7_strip4].
- A polygon of one triangle gets a triangle-array entry. More triangles get a strip entry with one tile mask bit per
  triangle [M].
- Every polygon writes its header and all its vertices again. With Strip_Len 1, each vertex after the second costs a
  whole triangle (226 ns) [M ta-perf strip_len1].
- An end of strip after two vertices raises "illegal parameter" and drops the strip [M err_short_strip].
- **[G]** Vertex positions (for binning) and vertex data (for parameters) are kept apart. After TA_LIST_INIT in a
  strip, one two-block listless unit counts as a single data entry but as three positions. That would explain the
  unmodelled continuations in i8_two and i8_after1.


## [5] Parameter FIFO and writer (the most surprising part)

Measured:

- **Two pointers.** TA_ISP_CURRENT is where each polygon is put (its entries' offset) and moves on by the
  polygon's size. A second, invisible pointer W is where the words actually go. The two only part when a polygon's
  size is not its number of words [M s6_*, s7_*].
- **Writes are held.** Words reach VRAM only below TA_ISP_CURRENT. The rest wait, in order, across polygons and
  lists, until a later polygon moves TA_ISP_CURRENT on. SOFTRESET and TA_LIST_INIT drop them; TA_LIST_CONT keeps
  them [M s7_isp_after, s7_cont_after, s7_reset_after].
- **Size from a word.** A polygon's size, and its entries' skip, come from the word at TA_ISP_CURRENT read as an ISP
  word: Texture bit 25, Offset 24, 16-bit UV 22. Normally that word is the polygon's own ISP word. Once the
  pointers have parted it is whatever word is there, such as a sprite's D.x or the TCW of a strip's second triangle
  [M s6_dx*, s7_dx*, s7_strip4]. In a modifier volume list the skip is always 0, so a sprite counts 15 words
  whatever it writes [M spr_mv_*].
- **Single tile bit.** Bit 21 goes into the word at TA_ISP_CURRENT as it is written, set or cleared, even when that
  word is a colour or a coordinate [M s6_dx40, s7_two_tris, spr_mv_then_mv].
- **Padding.** At the end of a strip (and after a sprite or a listless triangle), parameters short of the polygon's
  end are filled with the last word. There is no padding between the polygons of one strip [M s7_dx160, s7_strip4,
  i8_newhdr m6].
- **Commit time.** TA_ISP_CURRENT moves only when a polygon ends: dumps between a strip's vertices show it
  unchanged [M i8_one m1-m3]. But after TA_LIST_INIT in the middle of a strip, the polygon's size counts only what
  came after the TA_LIST_INIT: 3 + 4 words for one vertex [M i8_newhdr, init_mid_hdr].
- **Dropped polygons.** A culled polygon costs as much as a drawn one (220 ns against 226 ns a triangle) and leaves
  nothing in memory [M ta-perf culled_*, ta-tests uclip_* dumps].
- **TA_ISP_LIMIT.** The word at the limit is written, then nothing more of the polygon. A polygon that starts there
  is dropped with "parameters full". The limit's low two bits do not count [M isp_limit_*].

Inferred RTL [I]:

- A **parameter FIFO** between the strip assembler and VRAM. Its read side is gated by `address < TA_ISP_CURRENT`.
  Its write side takes every word the polygon generates, before the bounds are known.
- A **size counter A** that runs inside the polygon: plus 3 (or 5) for a header or polygon start, plus 3 + skip
  (or 3 + 2 skip) for each vertex. At commit A is copied to TA_ISP_CURRENT; on a cull, A and the FIFO's write
  pointer roll back. TA_LIST_INIT resets A along with TA_ISP_CURRENT and W, which is why only the vertices after it
  count.
- At polygon start, the skip is read from the FIFO entry at address TA_ISP_CURRENT (one random-access read port).
  At commit, bit 21 of that same entry is patched. Normally that entry is the polygon's ISP word. The quirks above
  are what this does when the size accounting has gone wrong.
- A "repeat the held last word" path, used when a strip ends while W < TA_ISP_CURRENT.
- Depth: at least the largest polygon. An 8-vertex two-volume textured strip with offset is 5 + 8 x 11 = 93 words.
  The leftover words of a sprite in a modifier list come on top of that.

Cost [M ta-perf]: a triangle in one tile takes 69 ns + 10.5 ns per parameter word (about 7 clocks + 1 clock a word).
That holds for nine vertex formats to within 0.1 ns, so the writer stores about one word per PVR clock.


## [6] Bounds and binning

- The tile of a coordinate is floor(x / 32), taken from the float's bits. With the sign bit set (-0 too) it is tile
  -1; at 2^31 or more, inf or NaN it is past the grid [M ta-tests bin_*, fuzz_110, fuzz_154].
- Bounds are taken per triangle (for a strip's masks) and as their union, clamped to the grid. Binning is by
  bounding box, not by exact coverage [M every tile case]. Polygons off the grid are dropped in every clip mode
  [M uclip_mode1, ta-tests lfuzz_00072].
- User clip on the polygon's box: inside, outside, or mode 1 (parameters written, no entries). The single tile bit
  comes from the box before clipping [M uclip_*, uclip_inside_single].
- **Modifier volume box.** One accumulated box: each triangle adds its own bounds. A triangle whose header has the
  Volume bit goes to the box's tiles, and the next one starts a new box. The first other polygon after modifier
  volumes, of any list, takes an open box as its own bounds too, then closes it. TA_LIST_INIT keeps the box;
  SOFTRESET drops it [M mvleak_*, ta-tests all_lists, modvol_volume_bit]. **[I]** One box register with a valid bit
  sits on the binning path, and nobody clears it at the end of a list.


## [7] Tile walker and tile state

- **Tile state RAM** = TA_OL_POINTERS: 608 words (0x005f8600..0x005f8f7c), readable by the CPU. Bit 31 means the
  tile has an entry in this list. Bits 30/29 mean the last entry is an open quad/triangle array. Bits 28-25 hold that
  array's count - 1, bit 24 its shadow bit, bits 23-2 the entry's address (after a strip entry, the next free word),
  and bits 1-0 the skip's low bits. Neither SOFTRESET nor TA_LIST_INIT clears it; only bit 31 is cleared, at list
  start [M olp_*, stack_20*, sprite_bits_sh, two_vol_skip].
- 608 is 600 rounded up to 19 x 32. Tiles past 608 have no state: their strip entries go to the slot start, each on
  top of the last; their arrays are lost; the end word covers them [M grid_41x15, grid_61x10, ta-tests grid_64x16].
- **Hidden state [I].** The register holds no parameter offset, yet an open array's entry word has one, and the join
  rule compares parameter adjacency. So there is a second per-tile RAM, about 608 x 40 bits: offset, skip, and
  either the end offset or the polygon size.
- **Join rule.** A triangle or sprite joins the tile's open array if the type, shadow bit and skip match, the array
  has fewer than 16, its parameters follow the array's last polygon's directly, and it is the same number of words.
  A polygon that wrote parameters elsewhere in between breaks the array; a culled one does not
  [M stack_*, ta-tests lfuzz_00551]. An open array is written out only when the tile's next entry cannot join it,
  or when the list ends.
- **Blocks.** The last word of a block is recognised from the address bits, with no per-tile counter: a block handed
  out off a multiple of its size is cut short [M opb_unaligned_*]. Growing up and growing down have separate
  pointers. Growing down hands out a block at NEXT + opaque block size - S [M down_*, mode_change]. At TA_OL_LIMIT,
  the word goes to the limit itself and "object lists full" is raised at once [M ol_limit_*].
- **Order [I].** Row by row over the box: refta does it that way and matches the write-queue tests.
- **Cost [M ta-perf]**, per tile of a full-grid box:

  | primitive | per tile | clocks |
  |---|---|---|
  | sprite | 10.4 ns | 1.0 |
  | quad strip (one strip entry per tile) | 17.3 ns | 1.7 |
  | triangle, or modifier volume triangle | 20.6 ns | 2.1 |

  Block size changes the strip figure: 1.6 / 1.7 / 2.25 clocks with 8 / 16 / 32-word blocks. **[I]** That points at
  the VRAM side: a wider matrix slot stride means more page misses. **[G]** One tile per clock is the walker's own
  rate (sprites); strip entries are bound by object list writes; triangles pay a second state-RAM access per tile.
  Why triangles but not sprites would need that second access is open (see the end).
- **Overlap [M].** A quad over 1 tile and one over 2 x 2 tiles cost the same (284 ns, 28 clocks). Over 4 x 4 a quad
  costs 433 ns: more than 284, less than 284 + 16 x 17.3. **[I]** The walk of one polygon runs while the next one's
  vertices come in, through a short queue. Parameter and object list writes probably share one VRAM write path.


## [8] Object list write queue

- No object list word reaches memory while fewer than 31 wait. After a polygon, when 31 or more wait, all but 16 are
  written: words in the list's matrix before words in blocks, oldest first. The rest go at the end of the list,
  before the open arrays and end words [M cache_tiles_steps: 0 written after 30 quads, 15 after 32, 30 after 46;
  q48_*; cache_chain: counted in words; olp_down m0].
- **[I]** A 32-entry queue: 5-bit count, a mark at 31, drained to half, with drains scheduled between polygons.
  Matrix-first ordering means two queues, or a priority bit.
- **[G]** In q16_up the console writes another subset than this rule does: tiles 12 and 16-19 instead of 32-34,
  which alias at 1 KB. The queue may be indexed by address, like a small write-combining cache, rather than a plain
  FIFO.
- Where object lists lie on parameters, this order decides which survives [M ta-tests opb_none, lfuzz_01240,
  lfuzz_01913].


## [9] End-of-list pass

- Walks every tile of the grid: an open array is written out, then the end word. Tiles past 608 get the end word at
  their slot start. Then the queue drains and the list-done interrupt is raised [M].
- Cost: about 0.8 us (80 clocks) + 15.5 ns (1.55 clocks) per tile; a 40 x 15 grid takes 10 us [M ta-perf eol_rows].
- With 64 tiles across or 16 down, the pass writes nothing: no open arrays, no end words. Strip entries and links are
  still written [M grid_*x16, grid_64x*, grid_max_y_stack]. **[I]** The walk loop compares a 6-bit x and a 4-bit y
  counter against `num + 1`, which wraps to 0 at those sizes, so the loop exits at once.


## Registers: when they are read

| register(s) | taken | evidence |
|---|---|---|
| TA_OL_BASE, TA_ALLOC_CTRL, TA_GLOB_TILE_CLIP → the five matrix bases | at TA_LIST_INIT and TA_LIST_CONT | clip_change, alloc_change_op, list_cont_base |
| block size, direction, grid of a list | when that list begins | alloc_change, mode_change, clip_change |
| TA_NEXT_OPB_INIT, TA_OL_BASE | bits 4-0 ignored | ol_base_odd |
| TA_ISP_BASE → TA_ISP_CURRENT and W | TA_LIST_INIT (SOFTRESET makes TA_ISP_CURRENT 0); TA_LIST_CONT keeps both | list_cont_alloc, s7_* |
| TA_ISP_LIMIT, TA_OL_LIMIT | compared on every write | isp_limit_*, ol_limit_* |


## Throughput budget (DMA input)

| work | ns | PVR clocks |
|---|---|---|
| input, one 32 byte block (DMA bound) | 54 | 5.4 |
| a header block on its own | <= 54 | <= 5.4 |
| a header before every triangle, extra | 84 | 8.4 |
| a triangle in one tile: fixed | 69 | 6.9 |
| one parameter word written | 10.5 | 1.05 |
| strip vertex, Strip_Len 6 (strips of 32) | 81 | 8.1 |
| a culled triangle | 220 | 22 |
| a tile of a box: sprite / strip / triangle | 10.4 / 17.3 / 20.6 | 1.0 / 1.7 / 2.1 |
| end of list: fixed + per grid tile | 800 + 15.5 | 80 + 1.55 |

The store queues deliver a block every 267 ns from main memory (164 ns from the cache). That is slower than the TA
for everything but polygons over many tiles, so with store-queue input the TA is idle most of the time and only the
end of the list is left when the last block is in. The display being on, or the list being opaque, translucent or
punch-through, changes nothing [M]. Not measured: the TA running against a render that reads VRAM at the same time.


## Storage, roughly

| what | size |
|---|---|
| input FIFO | 8 x 256 bits |
| header and control registers: ISP/TSP/TCW/TSP1/TCW1, PCW fields, two face colours, user clip (4 x 6/4 bits), Strip_Len, list type, last header for listless vertices | a few hundred bits |
| volume box + valid bit | 4 x 7 + 1 bits |
| strip assembler: 8 vertices x (3 + up to 8 attribute words), plus tile coordinates | about 3 Kbit |
| parameter FIFO | at least 93 words, plus leftovers |
| tile state: visible TA_OL_POINTERS + hidden open-array state | 608 x 32 + 608 x about 40 bits |
| object list write queue | 32 x (24-bit address + 32-bit data) |
| pointers: TA_ISP_CURRENT, A, W, NEXT_OPB up and down, five matrix bases | about 10 x 24 bits |


## Open questions, and the probe that would answer each

1. **Parameter FIFO depth.** Send more and more sprites in a modifier list (each leaves 4+ words waiting), then one
   polygon, and dump. The point where the words stop following each other in order gives the depth.
2. **Object list queue: FIFO or address-indexed?** Run polygons whose tiles alias at 1 KB, 2 KB and 4 KB strides,
   with a dump after each, as q16_up did with one stride.
3. **Why a triangle costs 2 clocks a tile and a sprite 1.** Time full-grid triangles whose arrays cannot join
   (alternating skip) against ones that can; strips of 1 to 6 triangles over the full grid; sprites that cannot join.
4. **Tile walk order.** A single polygon over more than 31 tiles, with a dump right after it: which tiles' words
   were written first shows the walk order.
5. **The running size counter A.** Read TA_ISP_CURRENT after every vertex of a Strip_Len 6 strip: it should not
   move until the polygon ends (i8_one suggests so for listless units).
6. **The TA's own intake rate** for cheap blocks, which DMA hides. Sample SB_TFREM in a tight loop during a DMA of
   headers only.
7. **TA against the core.** Run ta-perf's cases while the core renders a scene that reads VRAM heavily.
8. **TA_LIST_INIT in a strip that goes on** (i8_two, i8_after1). A dump after every block of longer strips would
   test whether positions and data are kept apart.
