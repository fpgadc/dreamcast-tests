#!/usr/bin/env python3
"""refsw2-offline against the console on whole game frames: the polly2 dumps of dreamster.

    python3 polly2dumps.py [--dumps <folder>] [scene ...]

A dump is a frame caught in an emulator: pvr_regs_<scene>.bin (the 8 KB of 0x005F8000-0x005F9FFF) and
vram_<scene>.bin (video memory, 32 bit area order). dreamster's pvr-renderer replayed every dump on a console
and kept video memory after the render as vram_out_<scene>.bin. Here refsw2-offline renders the same dump
(regs; vram; render; save) and video memory is compared with the console's: all of it, and the framebuffer
the registers name pixel by pixel. Scenes: those named, else every dump with a console render (64; a few
seconds each, some longer), one refsw2-offline per core at a time (JOBS=n for n): all of them in about 6
seconds on 32 cores. Work in /tmp/pvr_tests/polly2dumps/<scene>.

sa_slow1-3 found where a filtered sample falls in a mip level below the full size (pvr-tests/mipbil)."""
import concurrent.futures
import os, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
NULLDC3 = os.environ.get("NULLDC3", os.path.join(HERE, "..", "nulldc3"))
TOOL = os.environ.get("OFFLINE_TOOL", "refsw3-offline")      # refsw2-offline: the old refsw2 core
OFFLINE = os.environ.get("OFFLINE", "/tmp/" + TOOL)
WORK = "/tmp/pvr_tests/polly2dumps"
BPP = {0: 2, 1: 2, 2: 2, 3: 2, 4: 3, 5: 4, 6: 4}       # FB_W_CTRL packmode: bytes a pixel


def framebuffer(v8, regs):
    """the pixels the render wrote, as rows of bytes (FB_W_SOF1, FB_W_LINESTRIDE, FB_X_CLIP, FB_Y_CLIP)"""
    R = lambda off: int(regs[off // 4])
    bpp = BPP.get(R(0x48) & 7)
    if bpp is None:
        return None
    sof, stride = R(0x60) & 0xffffff, (R(0x4c) & 0x1ff) * 8
    x0, x1 = R(0x68) & 0x7ff, (R(0x68) >> 16) & 0x7ff
    y0, y1 = R(0x6c) & 0x3ff, (R(0x6c) >> 16) & 0x3ff
    w, h = min(x1 + 1, stride // bpp), y1 + 1      # a clip past the line (sc_ingame: 640 wide, 641 clipped)
    h = min(h, (len(v8) - sof) // stride)
    if h <= y0 or w <= x0:
        return None
    rows = np.frombuffer(v8[sof:sof + stride * h], dtype=np.uint8).reshape(h, stride)[:, :w * bpp]
    return rows.reshape(h, w, bpp)[y0:, x0:]


def main():
    args = sys.argv[1:]
    dumps = os.path.join(HERE, "..", "dreamster", "polly2-rtl", "polly2-data", "dumps")
    if args[:1] == ["--dumps"]:
        dumps, args = args[1], args[2:]
    scenes = args or sorted(f[len("vram_out_"):-4] for f in os.listdir(dumps) if f.startswith("vram_out_"))
    b = subprocess.run(["bash", os.path.join(NULLDC3, TOOL, "build.sh"), OFFLINE], capture_output=True, text=True)
    if b.returncode:
        sys.exit(TOOL + ": build failed\n" + b.stdout[-400:] + b.stderr[-400:])
    jobs = int(os.environ.get("JOBS", os.cpu_count() or 1))

    def one(s):
        """a scene: its line of the report, and whether all of video memory is the console's"""
        w = os.path.join(WORK, s)
        os.makedirs(w, exist_ok=True)
        size = os.path.getsize(f"{dumps}/pvr_regs_{s}.bin")
        if size not in (8192, 32768):
            return f"{s}: not replayed: its register file is {size} bytes, not the 8 KB of 0x005F8000-0x005F9FFF", False
        cmd = f"regs {dumps}/pvr_regs_{s}.bin; vram {dumps}/vram_{s}.bin; render; save {w}"
        r = subprocess.run([os.path.join(OFFLINE, TOOL), "--lax", "-c", cmd], capture_output=True, text=True)
        if r.returncode or not os.path.exists(os.path.join(w, "vram.bin")):
            return f"{s}: {TOOL} failed: {(r.stderr or r.stdout).strip().splitlines()[-1:]}", False
        hw = np.fromfile(f"{dumps}/vram_out_{s}.bin", dtype=np.uint8)
        sw = np.fromfile(os.path.join(w, "vram.bin"), dtype=np.uint8)
        os.remove(os.path.join(w, "vram.bin"))
        words = int((hw.view(np.uint32) != sw.view(np.uint32)).sum())
        regs = np.fromfile(f"{dumps}/pvr_regs_{s}.bin", dtype=np.uint32)
        fh, fs = framebuffer(hw, regs), framebuffer(sw, regs)
        if fh is None:
            px = "framebuffer not read"
        else:
            n = fh.shape[0] * fh.shape[1]
            same = int((fh == fs).all(-1).sum())
            px = f"framebuffer {same} of {n} pixels ({100 * same / n:.2f} %)"
        return f"{s}: {'video memory identical' if words == 0 else f'{words} words of video memory differ'}; {px}", words == 0

    # one refsw2-offline per scene, as many at once as there are cores (JOBS=n: n); the report in scene order
    exact = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for line, same in pool.map(one, scenes):
            print(line, flush=True)
            exact += same
    print(f"{exact} of {len(scenes)} scenes: all of video memory = the console's")


if __name__ == "__main__":
    main()
