# Porting a test from pvrtest.h (KallistiOS's PVR driver) to pvrtest2.h (the PVR driven directly)

Read `pvr_params.h`, `pvrsetup.h`, `pvrtest2.h` and `softfp.h` first (pvrio.h only if you are curious), and
one finished port next to its original: `texfmt/main.c` and `texfmt/main_kos.c` (textures, palette),
`covmeas/main.c` (sprites, modifier volumes, generated tables), `fbpack/main.c` (written new).

## Rules

1. Keep the original as `<test>/main_kos.c` (copy it before you change anything). The new `main.c` includes
   `pvrtest2.h`, defines `NUM_PAGES`, `test_setup()`, `test_describe()`, `test_page(page)` as before.
2. No KallistiOS PVR calls or types (they are poisoned). Parameter words are set field by field with the
   structs of `pvr_params.h` (nulldc3's names). No "context" structs, no helper that hides defaults: a reader
   of the test must see every bit that matters.
3. No floating point arithmetic and no `float` variables. A float is an `f32b` (its bit pattern):
   - a constant: `F(0.25f)` (the compiler converts it), `F32B_ZERO`, `F32B_ONE`, `F32B_HALF`, or hex;
   - from integers: `f32b_int(n)` (exact up to 2^24), `sf_from_int`, `sf_ratio(p, q)`, `f32b_scaled(n, e)`;
   - arithmetic: `sf_add`, `sf_sub`, `sf_mul`, `sf_div`, `sf_scale`, `sf_neg` (IEEE results, integer code).
   Translate the original's float expressions operation by operation in the same order (`a + b * c` is
   `sf_add(a, sf_mul(b, c))`), so that the same bits come out as on the console before. Where the original
   used `double` constants or mixed double arithmetic, think about what was computed in which precision.
   Tables from a generator (`gen.py`) carry floats as bit patterns: see `covmeas/gen.py` `write()`.
4. Do not change anything in `common/`, in `run2.sh` or in nulldc3. If something is missing, put it in your
   test's `main.c` and say so in your report.
5. Do not run the emulator. Validation is the console and refsw2-offline.
6. The test must draw the same picture as before. That is checked against the console's old dumps.

## What replaces what

| pvrtest.h / KallistiOS | pvrtest2.h |
|---|---|
| `pvr_list_begin(PVR_LIST_TR_POLY)` / `pvr_list_finish()` | `ta_list_begin(ListType_Translucent)` / `ta_list_end()` |
| `pvr_poly_cxt_*` + `pvr_poly_compile` + `pvr_prim(&hdr)` | fill `PCW`, `ISP_TSP`, `TSP`, `TCW`, then `ta_polygon(pcw, isp, tsp, tcw)` |
| `pvr_vertex_t` + `pvr_prim` | `ta_vertex0(x, y, z, BaseCol, eos)` untextured; `ta_vertex3(x, y, z, u, v, BaseCol, OffsCol, eos)` textured |
| other vertex kinds | fill `TA_Vertex4` / `TA_Vertex9` / `TA_Sprite0` / `TA_Sprite1` / `TA_ModVol` (`pcw = pcw_vertex(eos)`), `ta_send(&v, sizeof(v))`; a kind that is missing: define the struct in your test |
| `quad(&hdr, x, y, w, h, z, argb)` | `ta_polygon(...)` then `quad0(x, y, w, h, z, argb)` (whole pixels); not whole pixels: four `ta_vertex0` with softfp |
| `quad_uv` / `quad_full` | `quad3(x, y, w, h, z, u0, v0, u1, v1, BaseCol, OffsCol)` or four `ta_vertex3` |
| `cell_x(c)`, `cell_y(c)` (floats) | `cell_px(c)`, `cell_py(c)` (ints), `f32b_int(...)` |
| `pvr_mem_malloc` + `pvr_txr_load` / `upload_raw` | `at = pvr_tex_alloc(bytes); pvr_tex_load(at, data, bytes);` and `tcw.TexAddr = at >> 3` |
| `pvr_txr_load_ex(.., PVR_TXRLOAD_16BPP / 8BPP)` | `pvr_tex_load_twiddled16` / `pvr_tex_load_twiddled8` |
| `pvr_set_pal_format`, `pvr_set_pal_entry` | `pvr_pal_format(PAL_RAM_CTRL_...)`, `pvr_pal_entry(i, value)` |
| `PVR_SET(reg, v)` | `pvrio_reg(<NAME>_addr, v)`, nulldc3 register names (list in `pvr_params.h`) |
| `PVR_GET` | not available: nothing is read back from the chip. Keep the value you wrote. |
| `pvr_set_bg_color(r, g, b)` | `pvr_frame.bg_color = 0x00RRGGBB` |
| fog functions | write `FOG_TABLE_START_addr + 4*i`, `FOG_DENSITY_addr`, `FOG_COL_RAM_addr`, `FOG_COL_VERT_addr`, `FOG_CLAMP_MAX_addr`, `FOG_CLAMP_MIN_addr` yourself, with precomputed values |
| `TEST_MODIFIERS`, `TEST_AUTOSORT` | the same macros, before including `pvrtest2.h` |
| `TEST_PIXEL_MODE PM_RGB565` / `PM_RGB555` | `#define TEST_FB_PACKMODE fb_packmode_565` / `fb_packmode_0555` (KallistiOS turned dithering ON in these modes: `#define TEST_FB_DITHER 1`) |
| `rng()`, `rng_state`, `label()`, `label_span()` | the same |

Registers that `test_page` used to poke every frame can be written in `test_setup` if they never change.
`pvr_setup` writes every register once (see the table in `pvrsetup.h`); `pvr_frame_render` writes
`FB_X_CLIP`, `FB_Y_CLIP`, `ISP_BACKGND_D`, `ISP_BACKGND_T`, `REGION_BASE`, `PARAM_BASE`, `FB_W_SOF1` every frame.

## The header words

What `cxt_plain()` and `cxt_textured()` of pvrtest.h made, as fields (everything not listed is 0):

    PCW      ParaType = ParamType_Polygon_or_Modifier_Volume, ListType, Group_En = 1, Strip_Len = 1,
             Gouraud = 1 (0 with PVR_SHADE_FLAT), Texture (textured), Offset (PVR_SPECULAR_ENABLE),
             UV_16bit (PVR_UVFMT_16BIT), Col_Type (packed unless the test set fmt.color),
             Shadow / Volume (modifier settings)
    ISP_TSP  DepthMode = DepthMode_Always, ZWriteDis = 1, CullMode = CullMode_None, Texture (textured)
    TSP      SrcInstr = BlendInstr_One, DstInstr = BlendInstr_Zero, FogCtrl = FogCtrl_None, UseAlpha = 1;
             textured also: ShadInstr = ShadInstr_Decal (PVR_TXRENV_REPLACE), FilterMode, MipMapD = 4,
             TexU / TexV = log2(size) - 3, IgnoreTexA = 0
    TCW      PixelFmt, ScanOrder = 1 if PVR_TXRFMT_NONTWIDDLED, VQ_Comp, MipMapped, StrideSel, PalSelect,
             TexAddr = address >> 3

KallistiOS's constants have the same numbers as the fields (depth compare 0..7, culling 0..3, blend 0..7,
fog 0..3, texture environment 0..3, filter: 0 point, 1 bilinear, 2 and 3 trilinear), but do not guess for
anything unusual: `bash common/kos_words.sh prog.c` builds a small PC program against KallistiOS's own
`pvr_poly_compile` etc. and prints the words it makes (the script's header shows how). Compare them with the
`ta` lines your port prints: `gcc -I common -o /tmp/t <test>/main.c && /tmp/t /tmp | grep '^ta' | head`.
A vertex is `e0000000` (`f0000000` at the end of a strip) followed by x y z ...; every list ends with a
polygon parameter without vertices and eight zero words (the harness sends those).

Sprites: `ParaType = ParamType_Sprite`, `UV_16bit = 1`; KallistiOS's sprite context blends with source alpha
outside the opaque list (see `covmeas/main.c`).

## Checking a port

    bash run2.sh <test> off hw=192.168.178.107     # about 10 s; console runs wait for each other
    python3 compare2.py <test> hw kos              # the console now against the console before the port
    python3 compare2.py <test> hw off              # the console against refsw2-offline

- `run2.sh` must say "the console and the offline renderer got the same input". If it does not, the PC and
  the SH4 computed something differently: a float sneaked in.
- `hw = kos` has to hold in every region. `kos` are the dumps of the original on the same console
  (`fit/kos_hw/<test>`), compared on the colour bits. If a few pixels differ, find out why (usually a float
  expression translated in another order or precision, or a header bit) and fix the port; if the difference
  is real and cannot be avoided, describe it exactly.
- `hw = off` is the test's result. It should be at least what the README says the emulator scored for this
  test ("N of M regions"); differences in the bits above the colour are new information (the old comparison
  did not see them): report them, do not hide them.
- A page that looks wrong: `page<n>.hw.raw` / `page<n>.off.raw` in `/tmp/pvr_tests/<test>` are 640x480
  little endian words 0x00RRGGBB (16 bit modes: `.hw16.raw`, 640x480 words of 16 bits).
- The regions are the test's labels: `/tmp/pvr_tests/<test>/labels.txt` must have as many lines as
  `fit/kos_hw/<test>/labels.txt`.

If the test has analysis scripts (`*.py`) that read the emulator's screenshots, make them read
`page<n>.off.raw` (kind `off`) instead; see `covmeas/read.py` for the pattern.
