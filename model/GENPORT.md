# Porting a test generator from Python to C++

**Why.** The generators of covmeas, uvspan and ops place their cases by what the hardware will do, and took that
from a Python copy of the hardware model (`ops/model.py`). The model exists once, in C++: the soft floating
point headers of refsw2 (`~/projects/fpgadc/nulldc3/libswirl/rend/refsw2/pvr_softfp.h`, `pvr_isp_edge.h`,
`pvr_isp_depth.h`, `pvr_tsp_plane.h`). The user decided: each generator becomes a C++ program that includes
those headers directly. **No Python-to-C++ bridge of any kind** (no ctypes, no subprocess that answers queries).

**Reference port, read it first:** `zmeas/gen.cpp`, `zmeas/zmeas.h`, `zmeas/read.cpp`, and `model/gen_common.h`
(JSON, random numbers, bit patterns, and the model asked about one pixel). The Python originals are still in
each test's folder (and in `fit/python_generators_before_cpp.tar.gz`).

## Rules

1. Write `<test>/gen.cpp`; `bash model/build.sh <test>` builds it into `<test>/gen` (flags: -O2 -std=c++17 -Wall
   -Wextra; it must compile without warnings). Include `"gen_common.h"`.
2. **Do not edit** `model/gen_common.h`, `model/build.sh`, `regen.py`, `measure.sh`, `archive_check.sh`, `run2.sh`,
   anything in `common/`, another test's folder, or anything under `nulldc3` — other people are working with them
   right now. A helper you miss goes into your test's folder; name it in your report.
3. **Hardware arithmetic only from the refsw2 headers**, through `gen_common.h` (`Model`, `Pixel`, `Coverage`,
   `Covered`, `IspDepth`, `SpanStart`, `TspPlane`, `TspDivide`, `TspAttr`, `TspColor`) or the classes themselves
   (`pvrisp::EdgeTile`, `pvrisp::DepthTile`, `pvrtsp::Plane`). Never write an operation of the model again.
   From `ops/model.py`:

   | Python | C++ |
   |---|---|
   | `covered(cfg, x, y, px, py, half, rows)` | `gen::Covered` / `gen::Coverage` (one Setup per polygon and tile, then `Inside(i, j)`) |
   | `isp_depth` | `gen::IspDepth` |
   | `span_start` | `gen::SpanStart` |
   | `tsp_plane(..., attr, xs)` | `gen::TspPlane` (start = the run's first column in the tile), or `pvrtsp::Plane` once per polygon and `Value(...)` per pixel as `TspPlane` does |
   | `tsp_divide`, `tsp_attr`, `tsp_color` | `gen::TspDivide`, `TspAttr`, `TspColor` |
   | `edge_values` | `pvrisp::EdgeTile` after `Setup` (see `gen::Coverage::Setup` for the arguments): the value at pixel (i, j) of the tile is `t0 + tx * i + ty * j`, in half units, already turned so that inside is positive; `zero2` is the zero zone in the same units, which is the Python model's 64 G, so a Python threshold of `n` G is `n * zero2 / 64`. A tile with `constant` set has no values: skip that candidate |
   | `BASE` and alternative configs | `gen::Model` (default-constructed = the fitted model); an alternative is a copy with one field of `edge` / `depth` / `plane` changed |

4. Same command line (argument names and defaults), same mixes and distributions, **same `cases.json` schema** (key
   names, nesting, and integers stay integers: `Json(int)` versus `Json(double)`), and **the same `cases.h` text
   except the first comment line**. Random numbers come from `gen::Rng` seeded with `--seed`. numpy's streams are
   not reproduced: a seed gives other cases now, and that is accepted. `np.round` is `nearbyint` (half to even),
   a float32 conversion is `gen::F32`, a step of k last places is `gen::Step`.
5. Two modes. `gen [arguments]` writes `cases.h` and `cases.json` into its own folder (`gen::Here()`) and prints
   the summary the Python printed. `gen --regen <set>.cases.json <folder>` writes `<folder>/cases.h` and
   `<folder>/cases.json` from an archived set (`regen.py` calls it for `run2.sh <test>@<set>`). If the Python
   `write()` remade something from the seed with numpy, that cannot be done any more: store it in the archived
   `cases.json` once, with the old Python (what was done for the "tris" of zmeas scenes), and say so.
6. Port the docstring and the comments: they are the description of the test. Write them for a reader who has
   never seen the Python.
7. Your shell tool may turn `\` into `\` inside heredocs. After writing a file, grep it for string escapes and
   compile; `gen_common.h` avoids the issue with `static const char Backslash = 92`.
8. Do not delete the Python originals, do not commit anything, do not run the emulator. Keep runs short.

## Gates (report the numbers for each)

- **A, tables.** For every archived set (`fit/<test>_data/*.cases.json`): `gen --regen` gives a `cases.h` equal to
  the Python's, kept in `/tmp/genref/<test>/<set>/cases.h`, from line 2 on
  (`diff <(tail -n +2 new) <(tail -n +2 ref)`), and a `cases.json` that Python loads to the same data as the
  archive (`json.load(a) == json.load(b)`).
- **B, archives.** `bash archive_check.sh <test>` prints what it printed before the port
  (`/tmp/genref/archive_check.<test>.txt`).
- **C, new sets on the console.** For every mix (and the main `--prim` / `--list` variants) make a set with the
  C++ generator and measure it: `bash measure.sh <test> <name> <generator arguments>` (names start with `c`, e.g.
  `clattice31`; the console is at the default address; `run2.sh` takes turns on the console by itself; a run is
  10 to 40 seconds). Wanted: "the console and the offline renderer got the same input"; refsw2-offline equal
  to the console to the degree the Python-made sets of that mix are in the baseline of gate B; and **the mix
  does what it is for**: count it (for example how many edges pass through the zero zone of a pixel center)
  and compare with a set the old Python generator makes with the same arguments (run `gen.py` in a temporary
  copy of the folder; compare the statistics, not the cases).

## Report

Files written; the result of each gate with numbers; every deviation from the Python and why; what you missed in
`gen_common.h`; anything about the test or the model that looked wrong while porting.
