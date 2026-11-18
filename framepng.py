"""framepng: a framebuffer read out of a PVR (dumprender's frame.hw.raw / frame.off.raw) as a PNG, and back, bit exact.

    python3 framepng.py png <frame.raw> <dump> <out.png>     dump: <scene>.pvrdmp, or pvr_regs_<scene>.bin
    python3 framepng.py raw <in.png> <out.raw>               the framebuffer's bytes again
    python3 framepng.py check <frame.raw> <dump>             png and back in memory: the same bytes or not

The frame is FB_W_LINESTRIDE * 8 bytes a line, FB_Y_CLIP's lines, in FB_W_CTRL's pack mode (its registers come from
the dump). The PNG is width stride / bytes per pixel, the colours widened to 8 bits by repeating their top bits (so
the top bits are the stored ones again): RGB, or RGBA where the format has an alpha (4444, 1555, 8888). The K of
0555 and 0888 is FB_W_CTRL's fb_kval as the console writes it; it is kept as the PNG's text "pvr.k" when it is the
same everywhere, else as the alpha. The text chunks pvr.packmode, pvr.stride (and pvr.k) say how to undo it."""
import sys
import zlib
import numpy as np
from PIL import Image, PngImagePlugin

BPP = {0: 2, 1: 2, 2: 2, 3: 2, 4: 3, 5: 4, 6: 4}


def regs_of(dump):
    if dump.endswith(".pvrdmp"):
        d = zlib.decompressobj(31)
        d.decompress(open(dump, "rb").read()[11:])
        return np.frombuffer(zlib.decompressobj(31).decompress(d.unused_data), dtype="<u4")
    return np.fromfile(dump, dtype="<u4")


def widen(v, bits):
    v = v.astype(np.uint16)
    out = v << (8 - bits)
    k = bits
    while k < 8:                                   # repeat the top bits down to bit 0
        out |= (v << (8 - bits)) >> k
        k += bits
    return out.astype(np.uint8)


def to_png(raw, packmode, stride):
    bpp = BPP[packmode]
    if stride % bpp:
        raise ValueError(f"line stride {stride} is not a whole number of {bpp} byte pixels")
    b = np.frombuffer(raw, dtype=np.uint8)
    h = len(b) // stride
    b = b[:h * stride].reshape(h, stride)
    w = stride // bpp
    info = PngImagePlugin.PngInfo()
    info.add_text("pvr.packmode", str(packmode))
    info.add_text("pvr.stride", str(stride))
    alpha = None
    if bpp == 2:
        p = b.view("<u2").reshape(h, w).astype(np.uint32)
        if packmode == 1:
            rgb = [widen(p >> 11 & 31, 5), widen(p >> 5 & 63, 6), widen(p & 31, 5)]
        elif packmode == 2:
            rgb = [widen(p >> 8 & 15, 4), widen(p >> 4 & 15, 4), widen(p & 15, 4)]
            alpha = widen(p >> 12 & 15, 4)
        else:                                      # 0555 KRGB, 1555 ARGB
            rgb = [widen(p >> 10 & 31, 5), widen(p >> 5 & 31, 5), widen(p & 31, 5)]
            alpha = ((p >> 15 & 1) * 255).astype(np.uint8)
    elif bpp == 3:
        p = b.reshape(h, w, 3)
        rgb = [p[..., 2], p[..., 1], p[..., 0]]
    else:
        p = b.reshape(h, w, 4)
        rgb = [p[..., 2], p[..., 1], p[..., 0]]
        alpha = p[..., 3]
    if alpha is not None and packmode in (0, 5):   # K: one value everywhere goes into the text
        if (alpha == alpha.flat[0]).all():
            info.add_text("pvr.k", str(int(alpha.flat[0])))
            alpha = None
    a = np.dstack(rgb + ([alpha] if alpha is not None else []))
    return Image.fromarray(a, "RGBA" if alpha is not None else "RGB"), info


def to_raw(img):
    t = img.text
    packmode, stride = int(t["pvr.packmode"]), int(t["pvr.stride"])
    a = np.asarray(img).astype(np.uint32)
    h, w = a.shape[:2]
    r, g, bl = a[..., 0], a[..., 1], a[..., 2]
    al = a[..., 3] if a.shape[2] == 4 else np.full((h, w), int(t.get("pvr.k", 0)), dtype=np.uint32)
    if packmode == 1:
        p = (r >> 3) << 11 | (g >> 2) << 5 | bl >> 3
    elif packmode == 2:
        p = (al >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | bl >> 4
    elif packmode in (0, 3):
        p = (al >> 7) << 15 | (r >> 3) << 10 | (g >> 3) << 5 | bl >> 3
    elif packmode == 4:
        return np.dstack([bl, g, r]).astype(np.uint8).tobytes()
    else:
        return np.dstack([bl, g, r, al]).astype(np.uint8).tobytes()
    return p.astype("<u2").tobytes()


def frame_png(raw_path, dump):
    r = regs_of(dump)
    return to_png(open(raw_path, "rb").read(), int(r[0x48 // 4]) & 7, (int(r[0x4c // 4]) & 0x1ff) * 8)


def main():
    if len(sys.argv) < 4 or sys.argv[1] not in ("png", "raw", "check"):
        print(__doc__)
        return 1
    cmd = sys.argv[1]
    if cmd == "png":
        img, info = frame_png(sys.argv[2], sys.argv[3])
        img.save(sys.argv[4], pnginfo=info, optimize=True)
    elif cmd == "raw":
        open(sys.argv[3], "wb").write(to_raw(Image.open(sys.argv[2])))
    else:
        import io
        img, info = frame_png(sys.argv[2], sys.argv[3])
        f = io.BytesIO()
        img.save(f, "png", pnginfo=info)
        f.seek(0)
        same = to_raw(Image.open(f)) == open(sys.argv[2], "rb").read()
        print("same" if same else "DIFFERENT")
        return 0 if same else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
