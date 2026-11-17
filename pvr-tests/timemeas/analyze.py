"""Tables from the timing runs of timemeas.

    python3 pvr-tests/timemeas/analyze.py [folder with timemeas_run*.log, default fit/timemeas_logs]

A run is the console's log of `bash run2.sh timemeas hw=<ip>` (/tmp/pvr_tests/timemeas/build/hw.log) with the
unprintable characters removed; several runs are combined by taking the least of every number. All times are
counts of the CPU's cycle counter: 5 ns. The core's clock is 100 MHz, so 2 counts are one clock.

Only one event is used: the TSP's end-of-render bit, which is the end of the tile's write-out, timed from the
write to STARTRENDER. The ISP's end-of-render bit is not the end of the ISP's work (it comes later when small
triangles are seen); it is printed in one table to show that. What something costs is always a difference
between two scenes:
  * ISP: objects that are not seen (behind the background polygon) against the background alone;
  * TSP: a scene against its twin with the same polygons behind the background. The twin's TSP work is the
    background polygon's: 32 runs of 32 pixels, time B. So "TSP time of the tile" = difference + B."""
import collections
import glob
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def load(folder):
    D = collections.defaultdict(dict)
    F, L = {}, {}
    for f in sorted(glob.glob(str(Path(folder) / "timemeas_run*.log"))):
        for line in open(f, errors="replace"):
            p = line.split()
            if not p:
                continue
            if p[0] == "F":
                F[int(p[1])] = " ".join(p[2:])
            elif p[0] == "L":
                L[int(p[1])] = " ".join(p[2:])
            elif p[0] == "T" and len(p) >= 13 and p[6] == "W":
                k, v = tuple(int(x) for x in p[2:6]), tuple(int(x) for x in p[7:13])
                old = D[p[1]].get(k)
                D[p[1]][k] = v if old is None else tuple(min(a, b) for a, b in zip(old, v))
    return D, F, L


def clk(c):
    return f"{c:8.1f} counts = {c / 2:7.1f} clocks"


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else HERE.parent.parent / "fit/timemeas_logs"
    D, F, L = load(folder)
    if not D:
        sys.exit(f"no timemeas_run*.log in {folder}")
    # formats that the full runs measured (a log of a few scenes may list formats added later)
    F = {f: n for f, n in F.items() if (f, 2, 0, 0) in D.get("bgonly", {})}
    T = lambda what, *k: D[what][tuple(k) + (0,) * (4 - len(k))][2]          # the TSP's bit
    I = lambda what, *k: D[what][tuple(k) + (0,) * (4 - len(k))][0]          # the ISP's bit
    extra = lambda what, *k: T(what, *k) - T(what + "H", *k)
    B = (T("bgonly", 0, 4) - T("bgonly", 0, 2)) / 2

    print("1. A tile with nothing but the background polygon (flat, untextured), alone in the region array")
    print(f"   TSP's bit after STARTRENDER, 32 bit framebuffer, pointers to all five lists (the modifier volume lists too): {T('bgonly', 0, 0)} counts")
    print("   pointers to the first n lists only (opaque, opaque modifier, translucent, translucent modifier, punch through):")
    print("      " + "  ".join(f"{n}: {T('lists', n)}" for n in range(6)))
    print("   framebuffer pack mode (the same with dither):  " + "  ".join(
        f"{name}: {T('wout', m, 1)}" for m, name in ((0, "0555"), (1, "565"), (2, "4444"), (3, "1555"), (4, "888"), (5, "0888"), (6, "8888"))))
    print("   8 tiles after one entry that draws nothing:    " + "  ".join(
        f"{name}: {T('wout', m, 8)}" for m, name in ((1, "565"), (4, "888"), (5, "0888"))))
    print("   framebuffer clip (0888) leaving all / 16 lines / 16 columns / 16x16 / nothing:  " + "  ".join(str(T("clip", k)) for k in range(5)))
    print("   entries in front of the tile's that draw nothing and are not written out (as KallistiOS has one):")
    t = [T("bgonly", 0, d) for d in (0, 1, 2, 4)]
    print(f"      0: {t[0]}   1: {t[1]} (+{t[1] - t[0]})   2: {t[2]} (+{t[2] - t[1]})   4: {t[3]} (+{(t[3] - t[2]) / 2:.1f} each)")
    print(f"   => B, the TSP's time for a tile of background: {clk(B)}, {B / 1024:.3f} counts a pixel")
    t = {k[1]: v[2] for k, v in D["tiles"].items() if k[0] == 0}
    print("   the first n tiles of the screen (0888), after one such entry:  " + "  ".join(f"{n}: {t[n]}" for n in sorted(t)))
    print("      per tile more:  " + "  ".join(f"{(t[n] - t[n // 2]) / (n / 2):.0f}" for n in sorted(t) if n > 1))
    w16 = (T("wout", 1, 8) - (T("bgonly", 0, 1) - (T("wout", 5, 1) - T("wout", 1, 1)))) / 7
    print(f"      the same with a 16 bit framebuffer: {w16:.0f} per tile more")
    print()

    print("2. The background polygon by format: TSP time per region array entry (from 2 against 4 entries in front), and")
    print("   what the first tile costs more than that (texels not read before)")
    print(f"   {'format':44s} {'per entry':>10s} {'a pixel':>8s} {'first tile':>11s}")
    for f in sorted(F):
        per = (T("bgonly", f, 4) - T("bgonly", f, 2)) / 2
        once = T("bgonly", f, 0) - T("bgonly", 0, 0) - (per - B)
        print(f"   {F[f]:44s} {per:10.1f} {per / 1024:8.3f} {once:+11.1f}")
    print("   with one untextured sprite over the tile, the background's format changes nothing in that tile: "
          + "  ".join(f"{F[f][:12]}: {T('bgcover', f, 0)}" for f in (0, 2, 16, 23)))
    print()
    print("3. ISP: objects that are not seen. (TSP's bit with N objects - with none) / N, and from 64 to 256 of them.")
    print("   behind: behind the background (its depth 1, theirs 0.5, compare GREATER); never: compare NEVER;")
    print("   pass: compare ALWAYS, and a sprite over the tile after them. The tile is alone in the region array.")
    names = ["triangle 8x8", "quad 8x8 (strip of 2)", "sprite 8x8", "textured quad 8x8", "textured quad, offset", "quad 1x1", "quad 32x32",
             "quad 32x1", "quad 1x32", "sprite 32x32", "sprite 1x1", "strip of 6 triangles", "triangle of half a pixel"]
    ns = (1, 2, 4, 8, 16, 32, 64, 128, 256)
    print(f"   {'object':26s} {'behind':>7s} {'never':>7s} {'pass':>7s}  {'clocks':>6s}  {'ISP bit':>7s}   behind, total for N = " + " ".join(f"{n:d}" for n in ns))
    for o, name in enumerate(names):
        sl = [(T(m, o, 256) - T(m, o, 64)) / 192 for m in ("behind", "never", "pass")]
        ib = (I("behind", o, 256) - I("behind", o, 64)) / 192
        tot = " ".join(f"{T('behind', o, n) - T('behind', o, 0):d}" for n in ns)
        print(f"   {name:26s} {sl[0]:7.2f} {sl[1]:7.2f} {sl[2]:7.2f}  {sl[0] / 2:6.1f}  {ib:7.2f}   {tot}")
    print("   with one entry in front that draws nothing, the TSP is busy with that entry's background while the ISP works:")
    print("   total for 16 / 64 / 256 quads 8x8:  " + " / ".join(str(T("behind", 1, n, 1) - T("behind", 1, 0, 1)) for n in (16, 64, 256))
          + "   against  " + " / ".join(str(T("behind", 1, n) - T("behind", 1, 0)) for n in (16, 64, 256)) + "  alone")
    floor = T("bgonly", 0, 1)
    def per(what, a, b=0, c=0):
        t64, t256 = D[what][(a, b, c, 64)], D[what][(a, b, c, 256)]
        return (t256[2] - t64[2]) / 192, (t256[0] - t64[0]) / 192, t64[2] - floor < 300
    def show(v):
        return f"{v[0]:.1f}" + ("*" if v[2] else "")
    hs = (1, 2, 4, 8, 12, 16, 17, 20, 24, 28, 32)
    print("   (the scenes below have that entry in front; * = the 64 objects were still within its time, the number is low)")
    print("   quad 1 wide, by height:       " + "  ".join(f"{h}:{show(per('isp', 1, h))}" for h in hs))
    print("   quad 32 wide, by height:      " + "  ".join(f"{h}:{show(per('isp', 32, h))}" for h in hs))
    print("   quad 8x8 by place (x,y):      " + "  ".join(f"({pl // 64},{pl % 64}):{show(per('isp', 8, 8, pl))}" for pl in (0, 12, 24, 1536, 1560)))
    print("   strip of k triangles:         " + "  ".join(f"{k}:{show(per('strip', k))}" for k in range(1, 7)))
    print("   sprite, textured sprite:      " + "  ".join(show(per("prim", k)) for k in (0, 1)))
    print("   triangle culled by winding / not / culled as small, by the ISP's bit:  " + "  ".join(f"{per('prim', k)[1]:.1f}" for k in (4, 5, 6)))
    print()

    print("4. TSP by format, opaque list: difference to the twin, in counts")
    print("   triangle: (1024 quads of 1x1 + B) / 1024: one seen triangle of one pixel (the other half of a quad covers none)")
    print("   span: (32 quads of 1x32 + B) / 1024: a run of one pixel, 32 different triangles on every line")
    print("   quad: one quad 32x32 over the tile; 32x1: 32 quads of 32x1 (64 triangles of 16 pixels)")
    print(f"   {'format':44s} {'triangle':>9s} {'span':>7s} {'quad':>7s} {'32x1':>7s} {'4x4':>7s} {'8x32':>7s}")
    for f in sorted(F):
        print(f"   {F[f]:44s} {(extra('tsp', f, 5) + B) / 1024:9.2f} {(extra('tsp', f, 4) + B) / 1024:7.2f} {extra('tsp', f, 2):7d} "
              f"{extra('tsp', f, 3):7d} {extra('tsp', f, 10):7d} {extra('tsp', f, 12):7d}")
    print()

    print("5. Runs of 4, 2 and 1 pixels from few triangles: sprites 32 tall, centred, each narrower than the one before")
    print("   (4, 8 and 17 sprites: 8, 15 and 32 runs a line). Difference to the twin; per run over the widest;")
    print("   and what a run then costs with its pixels")
    for name, kinds, f in (("untextured", (8, 7, 0), 0), ("565", (14, 13, 5), 2), ("565 with offset colour", (17, 16, 15), 4)):
        e4, e2, e1 = (extra("cascade", k) for k in kinds)
        one = extra("tsp", f, 1)
        px = (T("bgonly", f, 4) - T("bgonly", f, 2)) / 2048
        print(f"   {name:24s} one sprite {one:5d}   4 px: {e4:5d} ({(e4 - one) / 256:+5.2f} a run, {4 * px + (e4 - one) / 256:5.2f})"
              f"   2 px: {e2:5d} ({(e2 - one) / 448:+5.2f}, {2 * px + (e2 - one) / 448:5.2f})   1 px: {e1:5d} ({(e1 - one) / 1024:+5.2f}, {px + (e1 - one) / 1024:5.2f})")
    print()

    print("6. Many triangles on a line: a quad over the tile and K columns one pixel wide over it. Difference to the twin,")
    print("   and per column and line since the K before")
    ks = (0, 1, 2, 3, 4, 6, 8, 12, 14, 16, 20, 24, 28, 32)
    for v, name in enumerate(("untextured quads", "untextured sprites", "565 Gouraud quads", "565 sprites", "565 sprites, offset colour")):
        e = [extra("comb", v, K) for K in ks]
        print(f"   {name:27s}" + " ".join(f"{K}:{x}" for K, x in zip(ks, e)))
        print(f"   {'':27s}" + " ".join(f"{(e[i] - e[i - 1]) / (ks[i] - ks[i - 1]) / 32:.1f}" for i in range(1, len(ks))))
    print("   columns' parameters moved apart in memory (1, 2, 3, 5 unseen quads between them), K = 16, untextured sprites:  "
          + "  ".join(str(extra("comb", 1, 16, p)) for p in (0, 1, 2, 3, 5)))
    print()
    print("7. The tile covered by polygons of w x h: TSP time per polygon, (difference to the twin + B) / number")
    S = (1, 2, 4, 8, 16, 32)
    for v, name in enumerate(("untextured sprites", "untextured quads (two triangles)", "565 Gouraud quads", "565 sprites")):
        print(f"   {name}")
        print("      h / w " + "".join(f"{w:9d}" for w in S) + "      the whole tile")
        for h in S:
            e = [extra("grid", v, w, h) + B for w in S]
            print(f"      {h:5d} " + "".join(f"{x / (1024 // (w * h)):9.2f}" for w, x in zip(S, e)) + "      " + " ".join(f"{x:6.0f}" for x in e))
    print()

    print("8. Cascades: difference to the twin.  For comparison:", extra("tsp", 0, 8), "for 1024 untextured sprites of 1x1")
    for k, text in ((1, "17 untextured quads, 32x32, 30x32 .. 2x32 centred and a 1x32: stripes, 1024 runs"),
                    (9, "the same 17 quads turned by a quarter (32x32, 32x30 ..): rows, one polygon a line"),
                    (0, "17 untextured sprites as stripes"), (10, "17 untextured sprites as rows"),
                    (4, "17 565 Gouraud quads as stripes"), (11, "17 565 Gouraud quads as rows"),
                    (5, "17 565 sprites as stripes"), (12, "17 565 sprites as rows"),
                    (2, "32 untextured sprites, left edges together (32x32, 31x32 ..): 1024 runs, one of each a line"),
                    (3, "the same with quads"),
                    (6, "16 untextured sprites 32x32, 32x30 .. centred: rows")):
        print(f"   {extra('cascade', k):6d}   {text}")
    print()

    print("9. Translucent layers. N quads on top of each other: per layer, (12 layers - 4) / 8, ISP and TSP together")
    for v in (0, 1):
        print("   " + ("untextured Gouraud " if v == 0 else "565 Gouraud        ") + "  ".join(
            f"{w}x{h}: {(T('blend', v, w, h, 12) - T('blend', v, w, h, 4)) / 8:7.1f}" for (w, h) in ((32, 32), (32, 16), (16, 16), (8, 8), (32, 1), (1, 1))))
    print("   every format as a translucent quad over the tile, per layer and pixel:")
    for f in sorted(F):
        if (f, 12, 0, 0) in D["layerfmt"]:
            print(f"      {F[f]:44s} {(T('layerfmt', f, 12) - T('layerfmt', f, 4)) / 8 / 1024:6.3f}")
    print()

    print("10. The ISP's end-of-render bit: how much later it comes than in the twin (the ISP's work is the same)")
    for Lk in (1, 2, 14, 12, 3, 10, 4, 5, 8):
        print(f"   {L[Lk]:20s} {I('tsp', 0, Lk) - I('tspH', 0, Lk):6d}    TSP's bit: {extra('tsp', 0, Lk):6d}")
    t = {k[1]: v for k, v in D["tiles"].items() if k[0] == 0}
    print("   n tiles of background: ISP's bit at  " + "  ".join(f"{n}: {t[n][0]}" for n in sorted(t)) + "   (TSP's at " + ", ".join(str(t[n][2]) for n in sorted(t)) + ")")

    print()

    print("11. Opaque modifier volumes. One sheet in front of everything, closed as INSIDE_LAST (the pixels under it are in")
    print("    the volume: cheap shadow, or the second parameters of polygons with two volumes) or as OUTSIDE_LAST (none is).")
    print("    The framebuffer shows the shadow in the first and not in the second; the ISP's work is the same.")
    PX = D.get("pixels", {})
    d = sorted((T("mv", f, Lk, 1) - T("mv", f, Lk, 2), f, Lk) for (f, Lk, m, z) in D["mv"] if m == 0)
    d2 = sorted((T("mv2", f, Lk, 1) - T("mv2", f, Lk, 2), f, Lk) for (f, Lk, m, z) in D["mv2"] if m == 0)
    print(f"    sheet over the tile, inside - outside: {len(d)} scenes of 6 formats and 11 layouts with cheap shadow: {d[0][0]} to {d[-1][0]} counts;")
    print(f"       {len(d2)} scenes of polygons with two volumes: {d2[0][0]} to {d2[-1][0]} counts")
    o = sorted(T("mv", f, Lk, 2) - T("mv", f, Lk, 0) for (f, Lk, m, z) in D["mv"] if m == 0)
    print(f"    outside - no volume at all (the ISP's work for the sheet's two triangles): {o[0]} to {o[-1]} counts, middle {o[len(o) // 2]}")
    print("    n volumes over the tile, inside / outside:  " + "   ".join(
        f"{n}: {T('mvn', 0, n, 1)} / {T('mvn', 0, n, 2)}" for n in (1, 2, 4, 8)) + f"   per volume more: {(T('mvn', 0, 8, 2) - T('mvn', 0, 1, 2)) / 7:.1f}")
    print("    polygon without the shadow bit, inside / outside: " + f"{T('mvflags', 0, 1)} / {T('mvflags', 0, 2)}"
          + "    one triangle as the volume, background: " + f"{T('mvtri', 0, 1)} / {T('mvtri', 0, 2)}")
    shapes = ((32, 32), (32, 16), (32, 8), (32, 4), (32, 2), (32, 1), (16, 32), (8, 32), (4, 32), (2, 32), (1, 32), (16, 16), (8, 8), (4, 4), (2, 2), (1, 1))
    print("    the sheet covers w x h at the tile's corner: inside - outside, counts")
    print("    " + " " * 34 + "".join(f"{w:>4d}x{h:<3d}" for w, h in shapes))
    for what, v, name in (("mvshape", 0, "background only"), ("mvshape", 1, "untextured sprite over the tile"), ("mvshape", 2, "565 sprite over the tile"),
                          ("mvshape", 3, "1024 untextured sprites of 1x1"), ("mv2shape", 0, "quad with two volumes"), ("mv2shape", 1, "565 Gouraud quad, two volumes")):
        print(f"    {name:34s}" + "".join(f"{T(what, v, w, h, 1) - T(what, v, w, h, 2):8d}" for w, h in shapes))
    px = B / 1024
    print("    a run of w pixels of the part in the volume, with its pixels: (difference + 32 w pixels) / 32, clocks")
    for what, v, name in (("mvshape", 0, "background only"), ("mvshape", 1, "untextured sprite over the tile"), ("mvshape", 2, "565 sprite over the tile"),
                          ("mv2shape", 0, "quad with two volumes"), ("mv2shape", 1, "565 Gouraud quad, two volumes")):
        print(f"    {name:34s}" + "   ".join(f"{w} px: {((T(what, v, w, 32, 1) - T(what, v, w, 32, 2)) / 32 + w * px) / 2:5.2f}" for w in (1, 2, 4, 8, 16)))

    print()

    print("12. Polygons with two volumes whose second volume blends differently, and the list they are in. A quad over the")
    print("    tile, first volume 0x404040 / 0x406080 / 0x804020 (no volume / inside / outside), second volume 0x202020")
    print("    (SRC_ALPHA: white with alpha 0x80). Pixel (20, 20) of the framebuffer after 12 renders, and the TSP's bit.")
    PXL = {}
    PXL0 = {}
    for f in sorted(glob.glob(str(Path(folder) / "timemeas_run*.log"))):
        for line in open(f, errors="replace"):
            p = line.split()
            if len(p) >= 16 and p[0] == "T" and p[13] == "P":
                PXL[(p[1],) + tuple(int(x) for x in p[2:6])] = p[15]
                PXL0[(p[1],) + tuple(int(x) for x in p[2:4])] = p[14]
    blends = ("as the first", "ONE, ONE", "ZERO, ONE", "SRC_ALPHA, INV_SRC_ALPHA")
    for l, lname in enumerate(("opaque list, opaque modifier volume", "punch through list, opaque modifier volume",
                               "translucent list (first volume's alpha 0x80), translucent modifier volume")):
        print(f"    {lname}")
        for b in range(4):
            k = l * 10 + b
            t = [T("mvb", k, 0, 2, m) for m in range(3)]
            t5 = [T("mvb", k, 0, 5, m) for m in range(3)]
            print(f"       second volume {blends[b]:26s} picture none / inside / outside: " + " / ".join(PXL[("mvb", k, 0, 2, m)] for m in range(3))
                  + f"    time {t[0]} / {t[1]} / {t[2]}   inside - outside {t[1] - t[2]:5d}   (1024 quads of 1x1: {t5[1] - t5[2]:6d})")
    print()
    print("13. Punch through polygons inside an opaque modifier volume are shaded twice: inside - outside is the TSP's work")
    print("    for them and nothing else. Polygons with two volumes (the second like the first), sheet over the tile.")
    print("    'twin': the same layout in the opaque list by the twin method (difference to the twin + B).")
    print(f"    {'format':32s} {'layout':16s} {'inside-outside':>14s} {'twin':>8s} {'outside-none':>13s}")
    for f in (0, 3, 4, 6, 16, 23):
        for Lk in (2, 12, 10, 3, 4, 5):
            d = T("mvp", f, Lk, 1) - T("mvp", f, Lk, 2)
            print(f"    {F[f][:32]:32s} {L[Lk]:16s} {d:14d} {extra('tsp', f, Lk) + B:8.0f} {T('mvp', f, Lk, 2) - T('mvp', f, Lk, 0):13d}")
    print("    per seen triangle (1024 quads of 1x1 / 1024), counts:  " + "  ".join(
        f"{F[f][:22]}: {(T('mvp', f, 5, 1) - T('mvp', f, 5, 2)) / 1024:.2f}" for f in (0, 3, 4, 6, 16, 23)))
    print("    with the shadow bit and cheap shadow instead of two volumes: inside - outside  " + "  ".join(
        f"{name}: {T('mvpc', t, Lk, 1) - T('mvpc', t, Lk, 2)}" for t, Lk, name in ((0, 1, "sprite"), (0, 2, "quad"), (0, 5, "1024 quads"), (1, 2, "565 quad"), (1, 5, "1024 565 quads"))))
    print("    the sheet covers w x h at the corner of one such quad over the tile: inside - outside, counts")
    print("    " + " " * 22 + "".join(f"{w:>4d}x{h:<3d}" for w, h in shapes))
    for v, name in ((0, "untextured"), (1, "565 Gouraud")):
        print(f"    {name:22s}" + "".join(f"{T('mvpshape', v, w, h, 1) - T('mvpshape', v, w, h, 2):8d}" for w, h in shapes))
    print("    translucent polygons with cheap shadow, sheet in the translucent modifier volume list: none / inside / outside")
    for t, Lk, name in ((0, 2, "untextured quad"), (0, 5, "1024 untextured quads"), (1, 2, "565 quad")):
        print(f"       {name:22s} " + " / ".join(str(T("mvtc", t, Lk, m)) for m in range(3)) + "   picture " + " / ".join(PXL[("mvtc", t, Lk, m, 0)] for m in range(3)))

    print()

    print("14. Translucent modifier volumes. A half transparent quad over the tile, 0x404040 / 0x406080 / 0x804020 with no")
    print("    sheet / inside / outside, over the background (0xc0c0c0) or over an opaque quad (0xffffff at the pixel read);")
    print("    a sheet in the translucent modifier volume list. Pixel (20, 20) and the TSP's bit for: no sheet / inside /")
    print("    outside / inside with the sheet behind the quad.")
    for kk, kname in enumerate(("no shadow bit", "shadow bit, cheap shadow", "two volumes, second 0x202020")):
        for o, oname in ((0, "background"), (1, "opaque quad")):
            for pp, pname in ((0, "sorted per pixel"), (1, "pre-sorted")):
                k = kk * 10 + o
                print(f"    {kname:28s} over the {oname:11s} {pname:16s} " + " / ".join(PXL[("mvtr", k, pp, m, 0)] for m in range(4))
                      + "    " + " / ".join(str(T("mvtr", k, pp, m)) for m in range(4)))
    print("    bit 0 of ISP_FEED_CFG set: the same pictures in " + str(sum(
        PXL[("mvtr", kk * 10 + o, pp + 2, m, 0)] == PXL[("mvtr", kk * 10 + o, pp, m, 0)] for kk in range(3) for o in range(2) for pp in range(2) for m in range(4))) + " of 48 scenes")
    print("    A sequence (a quad with two volumes; every render of the program before it was pre-sorted):")
    steps = sorted(k for k in D["mvtrs"])
    for (i, ps, m, o) in steps:
        print(f"       {i}: {'pre-sorted      ' if ps else 'sorted per pixel'}  {'sheet   ' if m else 'no sheet'}  {'over an opaque quad' if o else 'over the background'}"
              f"   picture {PXL[('mvtrs', i, ps, m, o)]}   time {D['mvtrs'][(i, ps, m, o)][2]}")

    print()

    print("15. Is the background drawn when the tile has no opaque list? Its colour changes from scene to scene (0xc0c0c0,")
    print("    0xc0ffc0, 0xffc0c0). Pixel (20, 20) and the TSP's bit, sorted per pixel / pre-sorted.")
    for v, name in enumerate(("pointers to five empty lists", "no list pointer at all", "no pointer to the opaque list, the others present",
                              "the same with a half transparent quad 0x406080 in the translucent list", "five pointers and that quad")):
        print(f"    {name:72s} " + "   ".join(" ".join(PXL[("bgcol", v, c, pp, 0)] for c in range(3)) for pp in (0, 1))
              + "    " + " / ".join(str(T("bgcol", v, 0, pp)) for pp in (0, 1)))

    print()

    print("16. The region array entry's \"keep Z\" bit. All entries are of tile (0, 0); 0888 framebuffer. Pixels (0, 0) and")
    print("    (20, 20) for the background colours 0xc0c0c0 / 0xc0ffc0 / 0xffc0c0 (rendered in that order), and the TSP's bit.")
    zk = ("the tile as usual, five empty lists", "keep Z, five empty lists", "keep Z, no list pointer",
          "an entry in front that draws nothing (not written out), then the tile", "that entry with keep Z, then the tile",
          "four such entries with keep Z, then the tile",
          "a white sprite over the tile; then a second entry, keep Z, a quad 0x406080 BEHIND it (GREATER)",
          "the same, the second entry without keep Z", "the same with keep Z and the quad in FRONT",
          "as the first of these three, the first entry not written out",
          "background only, not written out; then keep Z and a 16x16 quad 0x406080 at the corner",
          "the same, the first entry written out", "keep Z alone, the 16x16 quad", "keep Z alone, the white sprite over the tile",
          "the white sprite over the tile, as usual")
    for v, name in enumerate(zk):
        print(f"    {v:2d} {name:95s} " + "  ".join(PXL0[("zk", v, c)] + "," + PXL[("zk", v, c, 0, 0)] for c in range(3)) + f"   {T('zk', v, 0):5d}")
    print(f"    the background costs a tile {T('zk', 0, 0) - T('zk', 1, 0)} counts; an entry that draws nothing {T('zk', 3, 0) - T('zk', 0, 0)}, with keep Z "
          f"{T('zk', 4, 0) - T('zk', 0, 0)} ({(T('zk', 5, 0) - T('zk', 0, 0)) / 4:.0f} each of four); a write-out {T('zk', 6, 0) - T('zk', 9, 0)}")

    print()

    print("17. The palette's entry format (PAL_RAM_CTRL): the background polygon's TSP time per entry, counts a pixel")
    print(f"    {'format':44s} {'ARGB1555':>9s} {'RGB565':>9s} {'ARGB4444':>9s} {'ARGB8888':>9s}")
    for f in (15, 16, 21, 44, 42, 46, 3):
        print(f"    {F[f]:44s} " + " ".join(f"{(T('palfmt', m, f, 4) - T('palfmt', m, f, 2)) / 2048:9.3f}" for m in range(4)))

    print()

    print("18. Where the bilinear footprint starts: the background's texture stepped s texels a pixel along x, offset o.")
    print("    TSP time per entry, counts a pixel. s 2: o 0 = every footprint starts at an even x, o 1 = at an odd x;")
    print("    s 4: o 0 and 2 = odd, o 1 and 3 = even.")
    print(f"    {'format':40s} {'s1':>7s} {'s2 even':>8s} {'s2 odd':>8s} {'s4 odd':>8s} {'s4 even':>8s} {'s4 odd':>8s} {'s4 even':>8s}")
    for f in (20, 13, 6, 31, 32, 3, 40):
        print(f"    {F[f]:40s} " + " ".join(f"{(T('align', f, so, 4) - T('align', f, so, 2)) / 2048:8.3f}" for so in (10, 20, 21, 40, 41, 42, 43)))

    print()

    print("19. Where the bilinear footprint starts, in x and in y: the background's texture stepped along both. TSP time")
    print("    per entry, CLOCKS a pixel. e = the footprint starts at an even texel, o = at an odd one (x then y).")
    cols = ((1100, "1x1"), (2200, "2x2 ee"), (2210, "2x2 oe"), (2201, "2x2 eo"), (2211, "2x2 oo"), (4410, "4x4 ee"), (4400, "4x4 oe"),
            (4411, "4x4 eo"), (4401, "4x4 oo"), (2100, "2x1 e"), (2110, "2x1 o"), (4110, "4x1 e"), (4100, "4x1 o"))
    print(f"    {'':40s} " + " ".join(f"{name:>7s}" for key, name in cols))
    for c, name in enumerate(("YUV422 bilinear", "565 bilinear", "YUV422 point", "palette 8 bpp bilinear, 8888 entries", "palette 8 bpp bilinear, 565 entries",
                              "palette 8 bpp point, 8888 entries", "palette 8 bpp point, 565 entries")):
        print(f"    {name:40s} " + " ".join(f"{(T('align2', c, key, 4) - T('align2', c, key, 2)) / 4096:7.2f}" for key, nm in cols))


    if "dcalc" in D:
        print()
        print("20. DCalcCtrl (where the mipmap level's numerators are taken: 0 the screen's origin, 1 the first vertex).")
        print("    TSP work of the scene against its twin, clocks, the least of two passes; and the difference per seen")
        print("    triangle (1x1 quads: one of each quad's two triangles holds the pixel's center).")
        seen = {2: 2, 9: 32, 10: 128, 11: 512, 5: 1024}
        print(f"    {'format':32s} {'layout':16s} {'DCalc 0':>8s} {'DCalc 1':>8s} {'1 - 0':>7s} {'per triangle':>13s}")
        for f in (1, 3, 7, 8, 9):
            for lay in (2, 9, 10, 11, 5):
                w = [min(T("dcalc", f, lay, d, ps) - T("dcalcH", f, lay, d, ps) for ps in (0, 1)) / 2 for d in (0, 1)]
                print(f"    {F[f]:32s} {L[lay]:16s} {w[0]:8.0f} {w[1]:8.0f} {w[1] - w[0]:+7.0f} {(w[1] - w[0]) / seen[lay]:+13.2f}")


    if "dcalcs" in D:
        print()
        print("21. DCalcCtrl with sprites of w x h covering the tile: clocks per sprite, (scene - twin + 1035) / sprites,")
        print("    the least of the passes; and what DCalcCtrl 1 adds.")
        shapes = sorted({k[1] for k in D["dcalcs"]}, key=lambda v: ((v // 100) * (v % 100), v))
        for f in (0, 2):
            print(f"    {F[f]}")
            print(f"    {'w x h':>8s} {'DCalc 0':>8s} {'DCalc 1':>8s} {'extra':>7s}")
            for wh in shapes:
                n = 1024 // ((wh // 100) * (wh % 100))
                w = [min(T("dcalcs", f, wh, d, ps) - T("dcalcsH", f, wh, d, ps) for ps in (0, 1)) / 2 / n + 1035 / n for d in (0, 1)]
                print(f"    {wh // 100:>5d}x{wh % 100:<2d} {w[0]:8.1f} {w[1]:8.1f} {w[1] - w[0]:+7.2f}")


    if "dcalct" in D:
        print()
        print("22. DCalcCtrl, latency: the first n tiles, each covered by one primitive of its own, 16 bit framebuffer.")
        print("    TSP's bit in clocks for 1 and 64 tiles, and per tile ((64 - 1) / 63), DCalcCtrl 0 / 1.")
        for f in (0, 2, 7, 8):
            for q, qn in ((1, "sprite"), (2, "quad")):
                v = [[min(T("dcalct", f, 10 * n + q, d, ps) for ps in (0, 1)) / 2 for n in (1, 64)] for d in (0, 1)]
                per = [(x[1] - x[0]) / 63 for x in v]
                print(f"    {F[f]:32s} {qn:6s}  1 tile {v[0][0]:6.0f} / {v[1][0]:6.0f}   64 tiles {v[0][1]:7.0f} / {v[1][1]:7.0f}"
                      f"   per tile {per[0]:7.1f} / {per[1]:7.1f} ({per[1] - per[0]:+5.2f})")


    if "ahead2" in D:
        print()
        print("23. How far ahead the TSP sets up: in each of 64 tiles a 565 block with texels 8 apart over lines a .. 23 keeps")
        print("    the pixels busy, then N sprites of one pixel on line 24 need their setups at once. Per tile, clocks;")
        print("    'paying' = what DCalcCtrl 1 on the N sprites adds / 5 = sprites whose setup was not done ahead.")
        for a, name in ((0, "lines 0-23"), (1, "lines 16-23"), (2, "lines 20-23")):
            print(f"    block {name}")
            print(f"    {'N':>4s} {'DCalc 0':>8s} {'DCalc 1':>8s} {'paying':>7s} {'ahead':>6s}")
            for n in range(33):
                v = [T("ahead2", a, n, d) / 2 / 64 for d in (0, 1)]
                pay = (v[1] - v[0]) / 5
                print(f"    {n:4d} {v[0]:8.1f} {v[1]:8.1f} {pay:7.2f} {n - pay:6.2f}")


    if "spanpos" in D and "spanq" in D:
        print()
        print("24. Run starts ahead of the shading: a 565 triangle over each of 64 tiles, 15 one-pixel volume columns over")
        print("    L lines (30 runs a line). Extra clocks a tile over no cut, both passes averaged; c = clocks a run from")
        print("    L 4 .. 8; hidden = 30 L - extra / c.")
        def hidden(what, a, Ls):
            ex = {L: sum(T(what, a, L, ps) - T(what, a, 0, ps) for ps in (0, 1)) / 4 / 64 for L in Ls}      # clocks a tile
            c = (ex[8] - ex[4]) / 120
            return ex, c
        for B in (1, 8, 20):
            ex, c = hidden("spanpos", B, (1, 2, 3, 4, 6, 8))
            print(f"    cut from line {B:2d}: {c:.2f} a run; " + " ".join(f"L{L} +{ex[L]:.0f} ({30 * L - ex[L] / c:.0f})" for L in (1, 2, 3, 4, 6, 8)))
        for P in (1, 2, 4):
            ex, c = hidden("spanq", P, (4, 6, 8))
            print(f"    lines 0-19 in runs of {32 // P:2d}, cut from line 20: {c:.2f} a run; " + " ".join(f"L{L} +{ex[L]:.0f} ({30 * L - ex[L] / c:.0f})" for L in (4, 6, 8)))


    if "spanfmt" in D:
        print()
        print("25. What a run start costs, by format: one triangle over each of 64 tiles, 15 one-pixel volume columns over")
        print("    lines 0 .. L - 1 (31 runs a line). Clocks a run start, from the 8 lines added (both passes averaged).")
        for f in (0, 1, 2, 3, 4, 5):
            v = {L: sum(T("spanfmt", f, L, ps) for ps in (0, 1)) / 4 / 64 for L in (0, 8, 16, 24, 32)}
            segs = [(v[b] - v[a]) / 240 for a, b in ((0, 8), (8, 16), (16, 24), (24, 32))]
            print(f"    {F[f]:24s} lines 0-7 {segs[0]:5.2f}  8-15 {segs[1]:5.2f}  16-23 {segs[2]:5.2f}  24-31 {segs[3]:5.2f}")


    if "spanshad" in D:
        print()
        print("26. What the run pitch depends on: a 565 triangle cut into one-pixel runs over lines 0 .. L - 1; clocks a run")
        print("    start from lines 24-31 (both passes).")
        names = ["modulate", "decal", "modulate alpha", "offset, modulate", "offset, decal", "two volumes", "two volumes, offset"]
        for m, name in enumerate(names):
            v = {L: sum(T("spanshad", m, L, ps) for ps in (0, 1)) / 4 / 64 for L in (24, 32)}
            print(f"    {name:22s} {(v[32] - v[24]) / 240:5.2f}")


    if "ispq" in D:
        print()
        print("27. How far the ISP runs ahead of the TSP (ispq): n tiles, the TSP's bit minus the ISP's at n 64, the TSP's pace")
        print("    (n 32 -> 64), the lead in tiles of that pace, and the runs still to shade when the ISP ends (the lead less")
        print("    the last tile's write-out: 1357 at 0888, 1045 at 565).")
        names = ["background, 0888", "background, 565", "one sprite over the tile", "32 sprites 32x1 (rows)",
                 "4 sprites 8x32 (columns)", "32 sprites 1x32 (columns)", "4 sprites 32x8 (rows)",
                 "2 sprites 32x16 (rows)", "2 sprites 16x32 (columns)"]
        prims = [0, 0, 1, 32, 4, 32, 4, 2, 2]
        runs = [32, 32, 32, 32, 128, 1024, 32, 32, 64]
        wo = [1357, 1045] + [1357] * 7
        for k, name in enumerate(names):
            if (k, 64, 0, 0) not in D["ispq"]:
                continue
            gap = (T("ispq", k, 64) - I("ispq", k, 64)) / 2
            pace = (T("ispq", k, 64) - T("ispq", k, 32)) / 32 / 2
            left = (gap - wo[k]) / pace
            print(f"    {name:28s} {prims[k]:2d} primitives {runs[k]:4d} runs a tile: lead {gap:6.0f} clocks = {gap / pace:4.1f} tiles"
                  f" at {pace:5.0f} a tile; runs left {left * runs[k]:4.0f}")


    if "rows" in D:
        print()
        print("28. What limits 32 sprites of 32x1 a tile (rows): the pace a tile (n 32 -> 64), the TSP's bit and the ISP's,")
        print("    a row, and against plain untextured rows. E: primitives that never pass (ISP work only) added to each tile.")
        names = ["rows untextured", "rows 565", "rows 565 offset", "rows untextured, DCalcCtrl 1", "rows as quads",
                 "control: 4 rows of 32x8"]
        base = None
        for k, name in enumerate(names):
            for E in (0, 16, 32, 64):
                if (k, 64, E, 0) not in D["rows"]:
                    continue
                pace = (T("rows", k, 64, E) - T("rows", k, 32, E)) / 32 / 2
                isp = (I("rows", k, 64, E) - I("rows", k, 32, E)) / 32 / 2
                if base is None:
                    base = pace
                print(f"    {name:30s} E {E:2d}: {pace:7.1f} a tile ({pace / 32:5.1f} a row), ISP's bit {isp:7.1f}; {pace - base:+7.1f}")


    if "pfetch" in D:
        print()
        print("29. Is the rows' per-primitive resource a parameter fetch shared by the ISP and the TSP? Parameter size")
        print("    without setup work (an offset colour of 0: 4 more words a sprite, no TSP setup) and the write-out's")
        print("    VRAM traffic. Pace a tile (n 32 -> 64); lead: the TSP's end less the ISP's at 64 tiles.")
        names = [("rows untextured", 19), ("rows untextured, 565 framebuffer", 19), ("rows untextured, no write-out", 19),
                 ("rows 565", 23), ("rows 565, offset colour 0", 27), ("rows 565, offset colour", 27),
                 ("rows untextured + 32 never passing, 19 words", 19), ("rows untextured + 32 never passing, 27 words", 19)]
        pace = {}
        for k, (name, words) in enumerate(names):
            if (k, 64, 0, 0) not in D["pfetch"]:
                continue
            pace[k] = (T("pfetch", k, 64) - T("pfetch", k, 32)) / 32 / 2
            isp = (I("pfetch", k, 64) - I("pfetch", k, 32)) / 32 / 2
            lead = (T("pfetch", k, 64) - I("pfetch", k, 64)) / 2
            print(f"    {name:46s} {words:2d} words: {pace[k]:7.1f} a tile ({pace[k] / 32:5.1f} a row), ISP's bit {isp:7.1f},"
                  f" lead {lead:6.0f} ({lead / pace[k]:4.1f} tiles)")
        if len(pace) == 8:
            print(f"    16 bit framebuffer {pace[1] - pace[0]:+.1f} a tile, no write-out {pace[2] - pace[0]:+.1f}")
            print(f"    4 more words a passing sprite: {(pace[4] - pace[3]) / 32:+.2f} a row; the offset colour's setup:"
                  f" {(pace[5] - pace[4]) / 32:+.2f} a row")
            print(f"    never passing extras: {(pace[6] - pace[0]) / 32:.2f} each at 19 words, {(pace[7] - pace[0]) / 32:.2f}"
                  f" at 27 words")


    if "cut" in D:
        print()
        print("30. How fast the ISP's side cuts the tag buffer into runs: twins with the same sprites passing (ALWAYS) and")
        print("    never passing (NEVER); the ISP's work is the same, the runs are not. One tile; the ISP's bit (clocks).")
        place = [(1, 32, 1), (2, 32, 1), (4, 32, 1), (6, 32, 1), (4, 16, 1), (8, 8, 1), (16, 4, 0), (16, 8, 0)]
        def extra(K, h, x0):
            line = [0] * 32
            for j in range(K): line[x0 + 2 * j] = 1 + j
            runs = 1 + sum(1 for x in range(1, 32) if line[x] != line[x - 1])
            return (runs - 1) * h
        for k, (K, h, x0) in enumerate(place):
            if (k, 0, 0, 0) not in D["cut"]:
                continue
            a, b = I("cut", k, 0) / 2, I("cut", k, 1) / 2
            e = extra(K, h, x0)
            print(f"    {K:2d} sprites 1x{h:<2d}: {e:3d} more runs in {h:2d} lines: ISP {a:7.1f} against {b:7.1f}: {a - b:6.1f}"
                  f" ({(a - b) / e:4.2f} a run)")
    if "cutn" in D:
        print()
        print("    The first n tiles; one with the passing or never passing sprites (first or last), the others W sprites")
        print("    1x1 that never pass (ISP work only). The ISP's bit, ALWAYS less NEVER (clocks):")
        for k, e in ((1, 128), (2, 256)):
            for last in (0, 1):
                for n in (2, 3):
                    cells = []
                    for W in (0, 8, 16, 32):
                        if (k, W, n, 2 * last) in D["cutn"]:
                            cells.append(f"W {W:2d}: {(I('cutn', k, W, n, 2 * last) - I('cutn', k, W, n, 2 * last + 1)) / 2:6.1f}")
                    if cells:
                        print(f"    {e} more runs, {'last' if last else 'first'} of {n} tiles: " + "  ".join(cells))


if __name__ == "__main__":
    main()
